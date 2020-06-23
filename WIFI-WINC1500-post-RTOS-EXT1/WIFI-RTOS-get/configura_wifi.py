wifi_name = input('Wifi name: ')
wifi_pass = input('Wifi pass: ')
server_address = input('External Address:  ')


def replace_cfg(path):
    with open(path, 'r') as main:
        file = main.readlines()

    for i in range(len(file)):
        if (file[i].find('#define MAIN_WLAN_SSID') != -1):
            file[i] = "#define MAIN_WLAN_SSID \"${0}\"\n".format(wifi_name)
            file[i].replace(file[i], '#define main_wlan_ssid')
        if (file[i].find('#define MAIN_WLAN_PSK') != -1):
            file[i] = "#define MAIN_WLAN_PSK \"${0}\"\n".format(wifi_pass)
        if (file[i].find('#define MAIN_WLAN_PSK') != -1):
            file[i] = "#define MAIN_WLAN_PSK \"${0}\"\n".format(wifi_pass)

    file = "".join(file)
    # print(file)
    with open(path, 'w') as main:
        main.write(file)


replace_cfg("./src/main.h")

# define MAIN_WLAN_SSID                    "OnePlus 6"
# define MAIN_WLAN_AUTH                    M2M_WIFI_SEC_WPA_PSK
# define MAIN_WLAN_PSK                     "Guifi345"
