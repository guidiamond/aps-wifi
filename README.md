# aps-wifi

Para configurar sua rede, rodar o arquivo configura_wifi.py

Se quiser um servidor local, rodar python server.py
Se quiser um servidor online, pingar o ip do site aps-embarcados-wifi.herokuapp.com, que por ser dinamico, tem que ser colocado manualmente pelo usuário que não tiver o python instalado.

Em caso de sucesso no envio da mensagem, o led do Same70 ira piscar algumas vezes.

Como medida analógica, se esta usando um trimpot na porta ext2, igual ao do AFEC pin. (enviado para o server)

Como medida digital, o botão de usuário da placa. (que alterna o valor de button no json enviado ao server)
