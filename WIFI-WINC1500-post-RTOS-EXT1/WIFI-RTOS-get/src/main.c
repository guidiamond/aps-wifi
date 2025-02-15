#include "asf.h"
#include "main.h"
#include <string.h>
#include "bsp/include/nm_bsp.h"
#include "driver/include/m2m_wifi.h"
#include "socket/include/socket.h"
#include "util.h"
#include "defs.h"

typedef struct
{
	uint32_t year;
	uint32_t month;
	uint32_t day;
	uint32_t week;
	uint32_t hour;
	uint32_t minute;
	uint32_t second;
} calendar;



volatile char flag_rtc = 0;
volatile char but1_flag = 0;
/************************************************************************/
/* WIFI                                                                 */
/************************************************************************/

/** IP address of host. */
uint32_t gu32HostIp = 0;

/** TCP client socket handlers. */
static SOCKET tcp_client_socket = -1;

/** Receive buffer definition. */
static uint8_t g_receivedBuffer[MAIN_WIFI_M2M_BUFFER_SIZE] = {0};
static uint8_t g_sendBuffer[MAIN_WIFI_M2M_BUFFER_SIZE] = {0};

/** Wi-Fi status variable. */
static bool gbConnectedWifi = false;

/** Get host IP status variable. */
/** Wi-Fi connection state */
static uint8_t wifi_connected;

/** Instance of HTTP client module. */
static bool gbHostIpByName = false;

/** TCP Connection status variable. */
static bool gbTcpConnection = false;
static bool gbTcpConnected = false;

/** Server host name. */
static char server_host_name[] = MAIN_SERVER_NAME;

/** The conversion data is done flag */
volatile bool g_is_conversion_done = false;

/** The conversion data value */
volatile uint32_t g_ul_value = 0;

/************************************************************************/
/* RTOS                                                                 */
/************************************************************************/
void pisca_led(int n, int t, Pio* p, uint32_t mask) {
	for (int i = 0; i < n; i++) {
		pio_clear(p, mask);
		delay_ms(t);
		pio_set(p, mask);
		delay_ms(t);
	}
}

SemaphoreHandle_t xSemaphore, xSempahoreADC;
QueueHandle_t xQueueMsg;

extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,
signed char *pcTaskName);
extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);
extern void vApplicationMallocFailedHook(void);
extern void xPortSysTickHandler(void);


/************************************************************************/
/* HOOKs                                                                */
/************************************************************************/
/************************************************************************/
/* RTOS hooks                                                           */
/************************************************************************/

/**
* \brief Called if stack overflow during execution
*/
extern void vApplicationStackOverflowHook(xTaskHandle *pxTask,
signed char *pcTaskName)
{
	printf("stack overflow %x %s\r\n", pxTask, (portCHAR *)pcTaskName);
	/* If the parameters have been corrupted then inspect pxCurrentTCB to
	* identify which task has overflowed its stack.
	*/
	for (;;) {
	}
}

/**
* \brief This function is called by FreeRTOS idle task
*/
extern void vApplicationIdleHook(void)
{
	pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
}

/**
* \brief This function is called by FreeRTOS each tick
*/
extern void vApplicationTickHook(void)
{
}

extern void vApplicationMallocFailedHook(void)
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

	/* Force an assert. */
	configASSERT( ( volatile void * ) NULL );
}
/************************************************************************/
/* funcoes                                                              */
/************************************************************************/

/************************************************************************/
/* callbacks                                                            */
/************************************************************************/
/**
* \brief AFEC interrupt callback function.
*/
static void AFEC_pot_Callback(void){
	g_ul_value = afec_channel_get_value(AFEC_POT, AFEC_POT_CHANNEL);
	g_is_conversion_done = true;
}

void but_callback(void){
	but1_flag = 1 - but1_flag;
}
/**
* \brief Callback function of IP address.
*
* \param[in] hostName Domain name.
* \param[in] hostIp Server IP.
*
* \return None.
*/
static void resolve_cb(uint8_t *hostName, uint32_t hostIp)
{
	gu32HostIp = hostIp;
	gbHostIpByName = true;
	printf("resolve_cb: %s IP address is %d.%d.%d.%d\r\n\r\n", hostName,
	(int)IPV4_BYTE(hostIp, 0), (int)IPV4_BYTE(hostIp, 1),
	(int)IPV4_BYTE(hostIp, 2), (int)IPV4_BYTE(hostIp, 3));
}



/**
* \brief Callback function of TCP client socket.
*
* \param[in] sock socket handler.
* \param[in] u8Msg Type of Socket notification
* \param[in] pvMsg A structure contains notification informations.
*
* \return None.
*/
static void socket_cb(SOCKET sock, uint8_t u8Msg, void *pvMsg)
{
	/* Check for socket event on TCP socket. */
	if (sock == tcp_client_socket) {

		switch (u8Msg) {
			case SOCKET_MSG_CONNECT:
			{
				printf("socket_msg_connect\n");
				if (gbTcpConnection) {
					tstrSocketConnectMsg *pstrConnect = (tstrSocketConnectMsg *)pvMsg;
					if (pstrConnect && pstrConnect->s8Error >= SOCK_ERR_NO_ERROR) {
						printf("socket_cb: connect ok \n");
						gbTcpConnected = true;
						} else {
						printf("socket_cb: connect error!\r\n");
						gbTcpConnection = false;
						gbTcpConnected = false;
						close(tcp_client_socket);
						tcp_client_socket = -1;
					}
				}
			}
			break;

			case SOCKET_MSG_RECV:
			{
				char *pcIndxPtr;
				char *pcEndPtr;

				tstrSocketRecvMsg *pstrRecv = (tstrSocketRecvMsg *)pvMsg;
				if (pstrRecv && pstrRecv->s16BufferSize > 0) {
					xQueueSend(xQueueMsg, &pstrRecv, 10);
					xSemaphoreGive( xSemaphore );
					}  else {
					//printf("socket_cb: recv error!\r\n");
					close(tcp_client_socket);
					tcp_client_socket = -1;
				}
			}
			break;

			default:
			break;
		}
	}
}
void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type)
{
	/* Configura o PMC */
	pmc_enable_periph_clk(ID_RTC);

	/* Default RTC configuration, 24-hour mode */
	rtc_set_hour_mode(rtc, 0);

	/* Configura data e hora manualmente */
	rtc_set_date(rtc, t.year, t.month, t.day, t.week);
	rtc_set_time(rtc, t.hour, t.minute, t.second);

	/* Configure RTC interrupts */
	NVIC_DisableIRQ(id_rtc);
	NVIC_ClearPendingIRQ(id_rtc);
	NVIC_SetPriority(id_rtc, 0);
	NVIC_EnableIRQ(id_rtc);

	/* Ativa interrupcao via alarme */
	rtc_enable_interrupt(rtc, irq_type);
}

void RTC_Handler(void)
{
	uint32_t ul_status = rtc_get_status(RTC);
	printf("entrou");
	/*
	*  Verifica por qual motivo entrou
	*  na interrupcao, se foi por segundo
	*  ou Alarm
	*/
	if ((ul_status & RTC_SR_SEC) == RTC_SR_SEC) {
		rtc_clear_status(RTC, RTC_SCCR_SECCLR);
	}
	
	/* Time or date alarm */
	if ((ul_status & RTC_SR_ALARM) == RTC_SR_ALARM) {
		rtc_clear_status(RTC, RTC_SCCR_ALRCLR);
		flag_rtc = 1;
	}
	
	rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
	rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
	rtc_clear_status(RTC, RTC_SCCR_CALCLR);
	rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
}
/**
* \brief Callback to get the Wi-Fi status update.
*
* \param[in] u8MsgType Type of Wi-Fi notification.
* \param[in] pvMsg A pointer to a buffer containing the notification parameters.
*
* \return None.
*/
static void wifi_cb(uint8_t u8MsgType, void *pvMsg)
{
	switch (u8MsgType) {
		case M2M_WIFI_RESP_CON_STATE_CHANGED:
		{
			tstrM2mWifiStateChanged *pstrWifiState = (tstrM2mWifiStateChanged *)pvMsg;
			if (pstrWifiState->u8CurrState == M2M_WIFI_CONNECTED) {
				printf("wifi_cb: M2M_WIFI_CONNECTED\r\n");
				m2m_wifi_request_dhcp_client();
				} else if (pstrWifiState->u8CurrState == M2M_WIFI_DISCONNECTED) {
				printf("wifi_cb: M2M_WIFI_DISCONNECTED\r\n");
				gbConnectedWifi = false;
				wifi_connected = 0;
			}

			break;
		}

		case M2M_WIFI_REQ_DHCP_CONF:
		{
			uint8_t *pu8IPAddress = (uint8_t *)pvMsg;
			printf("wifi_cb: IP address is %u.%u.%u.%u\r\n",
			pu8IPAddress[0], pu8IPAddress[1], pu8IPAddress[2], pu8IPAddress[3]);
			wifi_connected = M2M_WIFI_CONNECTED;
			break;
		}

		case M2M_WIFI_RESP_GET_SYS_TIME:
		/*Initial first callback will be provided by the WINC itself on the first communication with NTP */
		{
			tstrSystemTime *strSysTime_now = (tstrSystemTime *)pvMsg;
			uint32_t u8week = (int) (strSysTime_now->u8Day/7)+1;
			
			calendar rtc_initial = {strSysTime_now->u16Year,strSysTime_now->u8Month,u8week,strSysTime_now->u8Hour,strSysTime_now->u8Minute,strSysTime_now->u8Second};
			RTC_init(RTC, ID_RTC, rtc_initial, RTC_IER_ALREN);

			/* Print the hour, minute and second.
			* GMT is the time at Greenwich Meridian.
			*/
			printf("socket_cb: Year: %d, Month: %d, The GMT time is %u:%02u:%02u\r\n",
			strSysTime_now->u16Year,
			strSysTime_now->u8Month,
			strSysTime_now->u8Hour-3,    /* hour (86400 equals secs per day) */
			strSysTime_now->u8Minute,  /* minute (3600 equals secs per minute) */
			strSysTime_now->u8Second); /* second */
			break;
		}

		default:
		{
			break;
		}
	}
}

static void config_AFEC_pot(Afec *afec, uint32_t afec_id, uint32_t afec_channel, afec_callback_t callback){
	/*************************************
	* Ativa e configura AFEC
	*************************************/
	/* Ativa AFEC - 0 */
	afec_enable(afec);

	/* struct de configuracao do AFEC */
	struct afec_config afec_cfg;

	/* Carrega parametros padrao */
	afec_get_config_defaults(&afec_cfg);

	/* Configura AFEC */
	afec_init(afec, &afec_cfg);

	/* Configura trigger por software */
	afec_set_trigger(afec, AFEC_TRIG_SW);

	/*** Configuracao espec�fica do canal AFEC ***/
	struct afec_ch_config afec_ch_cfg;
	afec_ch_get_config_defaults(&afec_ch_cfg);
	afec_ch_cfg.gain = AFEC_GAINVALUE_0;
	afec_ch_set_config(afec, afec_channel, &afec_ch_cfg);

	/*
	* Calibracao:
	* Because the internal ADC offset is 0x200, it should cancel it and shift
	down to 0.
	*/
	afec_channel_set_analog_offset(afec, afec_channel, 0x200);

	/***  Configura sensor de temperatura ***/
	struct afec_temp_sensor_config afec_temp_sensor_cfg;

	afec_temp_sensor_get_config_defaults(&afec_temp_sensor_cfg);
	afec_temp_sensor_set_config(afec, &afec_temp_sensor_cfg);
	
	/* configura IRQ */
	afec_set_callback(afec, afec_channel,	callback, 1);
	NVIC_SetPriority(afec_id, 4);
	NVIC_EnableIRQ(afec_id);
}
void post_msg(char *endpoint, char *data)
{
	size_t content_length = strlen(data);
	sprintf(g_sendBuffer, "POST %s HTTP/1.0\nContent-Type: application/json\nContent-Length: %d\n\n%s", endpoint, content_length, data);
	printf("MANDANDO POST - %s\n", g_sendBuffer);
	send(tcp_client_socket, g_sendBuffer, strlen((char *)g_sendBuffer), 0);
}
/************************************************************************/
/* TASKS                                                                */
/************************************************************************/

static void task_process(void *pvParameters) {

	printf("task process created \n");
	vTaskDelay(1000);

	uint msg_counter = 0;
	tstrSocketRecvMsg *p_recvMsg;

	enum states {
		WAIT = 0,
		GET,
		POST,
		ACK,
		MSG,
		TIMEOUT,
		DONE,
	};

	enum states state = WAIT;

	const usart_serial_options_t usart_serial_options = {
		.baudrate     = CONF_UART_BAUDRATE,
		.charlength   = CONF_UART_CHAR_LENGTH,
		.paritytype   = CONF_UART_PARITY,
		.stopbits     = CONF_UART_STOP_BITS
	};
	/* Initialize stdio on USART */
	stdio_serial_init(CONF_UART, &usart_serial_options);

	/* inicializa e configura adc */
	config_AFEC_pot(AFEC_POT, AFEC_POT_ID, AFEC_POT_CHANNEL, AFEC_pot_Callback);

	/* Selecina canal e inicializa convers�o */
	afec_channel_enable(AFEC_POT, AFEC_POT_CHANNEL);
	afec_start_software_conversion(AFEC_POT);
	while(1){
		
		switch(state){
			case WAIT: {
				// aguarda task_wifi conectar no wifi e socket estar pronto
				printf("STATE: WAIT \n");
				while(gbTcpConnection == false && tcp_client_socket >= 0){
					vTaskDelay(10);
				}
				state = POST;
				break;
			}

			case GET: {
				printf("STATE: GET \n");
				sprintf((char *)g_sendBuffer, MAIN_PREFIX_BUFFER);
				send(tcp_client_socket, g_sendBuffer, strlen((char *)g_sendBuffer), 0);
				state = ACK;
				break;
			}
			
			case POST: {
				uint32_t year, month, day, week, hour, seconds, minutes;
				char time[100];
				uint32_t trimpot_value = 0;
				if(g_is_conversion_done){
					trimpot_value = g_ul_value;
					delay_ms(500);
					
					/* Selecina canal e inicializa convers�o */
					afec_channel_enable(AFEC_POT, AFEC_POT_CHANNEL);
					afec_start_software_conversion(AFEC_POT);
				}
				rtc_get_date(RTC, &year, &month, &day, &week);
				rtc_get_time(RTC, &hour, &minutes, &seconds);
				sprintf(time, "\"%02d/%02d/%02d-%02d:%02d:%02d\"", year, month, day, hour, minutes, seconds);
				char formatted_json[2000];
				sprintf(formatted_json, "{\"timestamp\":%s,\"id\":%s, \"trimpot_value\":%d,\"button_status\":%d}", time,ID_PLACA,trimpot_value, but1_flag);
				post_msg("/status", formatted_json);
				state = ACK;
				break;
			}

			case ACK: {
				printf("STATE: ACK \n");
				memset(g_receivedBuffer, 0, MAIN_WIFI_M2M_BUFFER_SIZE);
				recv(tcp_client_socket, &g_receivedBuffer[0], MAIN_WIFI_M2M_BUFFER_SIZE, 0);
				
				if(xQueueReceive(xQueueMsg, &p_recvMsg, 5000) == pdTRUE){
					printf(STRING_LINE);
					printf(p_recvMsg->pu8Buffer);
					pisca_led(5, 50, LED_PIO, LED_IDX_MASK);
					printf(STRING_EOL);
					printf(STRING_LINE);
					state = MSG;
				}
				else {
					state = TIMEOUT;
				};
				break;
			}

			case MSG: {
				printf("STATE: MSG \n");
				memset(g_receivedBuffer, 0, MAIN_WIFI_M2M_BUFFER_SIZE);
				recv(tcp_client_socket, &g_receivedBuffer[0], MAIN_WIFI_M2M_BUFFER_SIZE, 0);
				
				if(xQueueReceive(xQueueMsg, &p_recvMsg, 5000) == pdTRUE){
					printf(STRING_LINE);
					printf(p_recvMsg->pu8Buffer);
					printf(STRING_EOL);  printf(STRING_LINE);
					state = DONE;
				}
				else {
					state = TIMEOUT;
				};
				break;
			}

			case DONE: {
				printf("STATE: DONE \n");
				state = WAIT;
				break;
				
				case TIMEOUT:
				state = WAIT;
				break;
				
				default: state = WAIT;
			}
		}
	}
}

void init_wifi(void) {
	tstrWifiInitParam param;

	xSemaphore = xSemaphoreCreateCounting(20,0);
	xQueueMsg = xQueueCreate(10, sizeof(tstrSocketRecvMsg));

	/* Initialize the BSP. */
	nm_bsp_init();

	/* Initialize Wi-Fi parameters structure. */
	memset((uint8_t *)&param, 0, sizeof(tstrWifiInitParam));

	/* Initialize Wi-Fi driver with data and status callbacks. */
	param.pfAppWifiCb = wifi_cb;
	int8_t ret = m2m_wifi_init(&param);
	if (M2M_SUCCESS != ret) {
		printf("main: m2m_wifi_init call error!(%d)\r\n", ret);
		while (1) { }
	}

	/* Initialize socket module. */
	socketInit();

	/* Register socket callback function. */
	registerSocketCallback(socket_cb, resolve_cb);
	/* Connect to router. */
	printf("main: connecting to WiFi AP %s...\r\n", (char *)MAIN_WLAN_SSID);
	m2m_wifi_connect((char *)MAIN_WLAN_SSID, sizeof(MAIN_WLAN_SSID), MAIN_WLAN_AUTH, (char *)MAIN_WLAN_PSK, M2M_WIFI_CH_ALL);

	
}

static void task_wifi(void *pvParameters) {
	init_wifi();
	/* formata ip */
	struct sockaddr_in addr_in;
	addr_in.sin_family = AF_INET;
	addr_in.sin_port = _htons(MAIN_SERVER_PORT);
	inet_aton(MAIN_SERVER_NAME, &addr_in.sin_addr);

	printf(STRING_LINE);
	while(1){
		vTaskDelay(50);
		m2m_wifi_handle_events(NULL);

		if (wifi_connected == M2M_WIFI_CONNECTED) {
			/* Open client socket. */
			if (tcp_client_socket < 0) {
				printf(STRING_LINE);
				printf("socket init \n");
				if ((tcp_client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
					printf("main: failed to create TCP client socket error!\r\n");
					continue;
				}

				/* Connect server */
				printf("socket connecting\n");
				if (connect(tcp_client_socket, (struct sockaddr *)&addr_in,
				sizeof(struct sockaddr_in)) != SOCK_ERR_NO_ERROR) {
					close(tcp_client_socket);
					tcp_client_socket = -1;
					printf("main: error connect to socket\n");
					}else{
					gbTcpConnection = true;
				}
			}
		}
	}
}
void init_but_and_led() {
	pmc_enable_periph_clk(BUT_PIO_ID);
	pmc_enable_periph_clk(LED_PIO_ID);
	pio_configure(BUT_PIO, PIO_INPUT, BUT_PIO_IDX_MASK, PIO_PULLUP|PIO_DEBOUNCE);
	pio_configure(LED_PIO, PIO_OUTPUT_1, LED_IDX_MASK, PIO_DEFAULT);
	
	pio_handler_set(BUT_PIO, BUT_PIO_ID, BUT_PIO_IDX_MASK, PIO_IT_RISE_EDGE, (void (*)(uint32_t, uint32_t)) but_callback);
	
	
	
}

void init(void) {
	/* Initialize the board. */
	sysclk_init();
	board_init();
	init_but_and_led();
	configure_console();
	printf(STRING_HEADER);
	// Desativa watchdog
	WDT->WDT_MR = WDT_MR_WDDIS;
}
void create_tasks() {
	if (xTaskCreate(task_wifi, "Wifi", TASK_WIFI_STACK_SIZE, NULL, TASK_WIFI_PRIORITY, NULL) != pdPASS) {
		printf("Failed to create wifi task\r\n");
	}
	if (xTaskCreate(task_process, "process", TASK_PROCESS_STACK_SIZE, NULL, TASK_PROCESS_PRIORITY,  NULL) != pdPASS) {
		printf("Failed to create process task\r\n");
	}

	vTaskStartScheduler();
}
int main(void)
{
	init();
	create_tasks();
	while(1) {};
	return 0;
}
