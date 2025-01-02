// This translation file is maintained on https://crowdin.com/project/openhasp/
// Do not edit directly!

#ifndef HASP_LANG_ES_ES_H
#define HASP_LANG_ES_ES_H

#define D_ISO_LANG_CODE "es-ES"

#define D_USERNAME "Usuario:"
#define D_PASSWORD "Contraseña:"
#define D_SSID "Ssid:"
#define D_YES "Si" // New
#define D_NO "No"  // New

#define D_ERROR_OUT_OF_MEMORY "Memory llena"
#define D_ERROR_UNKNOWN "Error desconocido"

#define D_CONFIG_NOT_CHANGED "No hay cambios en la configuracion"
#define D_CONFIG_CHANGED "Configuracion cambiada"
#define D_CONFIG_LOADED "Configuracion cargada"

#define D_FILE_LOADING "Cargando %s"
#define D_FILE_LOADED "%s cargado"
#define D_FILE_LOAD_FAILED "No se pudo cargar %s"
#define D_FILE_SAVING "Guardando %s"
#define D_FILE_SAVED "%s guardado"
#define D_FILE_SAVE_FAILED "No se pudo guardar %s"
#define D_FILE_NOT_FOUND "Archivo no encontrado"
#define D_FILE_SIZE_BYTES "bytes"   // new
#define D_FILE_SIZE_KILOBYTES "KiB" // new
#define D_FILE_SIZE_MEGABYTES "MiB" // new
#define D_FILE_SIZE_GIGABYTES "GiB" // new
#define D_FILE_SIZE_TERABYTES "TiB"
#define D_FILE_SIZE_DIVIDER 1024    // new, kibi or kilo bytes
#define D_DECIMAL_POINT ","         // new, decimal comma or point

#define D_SERVICE_STARTING "Inicializando..."
#define D_SERVICE_STARTED "Inicializado"
#define D_SERVICE_START_FAILED "No se pudo arrancar"
#define D_SERVICE_STOPPED "Parado"
#define D_SERVICE_DISABLED "Deshabilitado"
#define D_SERVICE_CONNECTED "Conectado"
#define D_SERVICE_DISCONNECTED "Desconectado"

#define D_SETTING_ENABLED "Habilitado"     // New
#define D_SETTING_DISABLED "Deshabilitado" // New
#define D_SETTING_DEFAULT "Default"        // New

#define D_NETWORK_IP_ADDRESS_RECEIVED "Se recibio la direccion IP: %s"
#define D_NETWORK_ONLINE "en linea"
#define D_NETWORK_OFFLINE "fuera de línea"
#define D_NETWORK_CONNECTION_FAILED "Fallo la conexion"
#define D_NETWORK_CONNECTION_UNAUTHORIZED "Fallo la autorizacion"

#define D_MQTT_DEFAULT_NAME "placa_%s"
#define D_MQTT_CONNECTING "Conectando..."
#define D_MQTT_CONNECTED "Conectado al Broker %s con el clientID %s"
#define D_MQTT_NOT_CONNECTED "No hay conexion ???"
#define D_MQTT_DISCONNECTING "Desconectando..."
#define D_MQTT_DISCONNECTED "Desconectado"
#define D_MQTT_RECONNECTING "Desconectado del broker, reconectando..."
#define D_MQTT_NOT_CONFIGURED "No se ha configurado el Broker"
#define D_MQTT_STARTED "Arrancando: %d bytes"
#define D_MQTT_FAILED "Fallo:"
#define D_MQTT_INVALID_TOPIC "El mensaje tiene un topico invalido"
#define D_MQTT_SUBSCRIBED "Subscrito a %s"
#define D_MQTT_NOT_SUBSCRIBED "No se pudo subscribir a %s"
#define D_MQTT_HA_AUTO_DISCOVERY "Registrando auto-descubrimiento en HA"
#define D_MQTT_PAYLOAD_TOO_LONG "Los datos enviados son demasiado largos(%u bytes)"

#define D_TELNET_CLOSING_CONNECTION "Cerrando sesion de %s"
#define D_TELNET_CLIENT_LOGIN_FROM "Se ha firmado el cliente %s"
#define D_TELNET_CLIENT_CONNECT_FROM "Se ha conectado el cliente %s"
#define D_TELNET_CLIENT_NOT_CONNECTED "Cliente NO conectado"
#define D_TELNET_INCORRECT_LOGIN_ATTEMPT "Intento de conexion incorrecta desde %s"
#define D_TELNET_STARTED "Console Telnet arrancada"
#define D_TELNET_FAILED "Fallo el arranque de la consola Telnet"
#define D_TELNET_CLIENT_CONNECTED "Cliente conectado"
#define D_TELNET_CLIENT_REJECTED "Cliente rechazado"

#define D_HASP_INVALID_PAGE "Pagina invalida %u"
#define D_HASP_INVALID_LAYER "No se puede borrar una capa del sistema"
#define D_HASP_CHANGE_PAGE "Cambiando a pagina %u"
#define D_HASP_CLEAR_PAGE "Limpiando pagina %u"

#define D_OBJECT_DELETED "Objeto borrado"
#define D_OBJECT_UNKNOWN "Objeto desconocido"
#define D_OBJECT_MISMATCH "Los objetos NO SON IGUALES!"
#define D_OBJECT_LOST "Objeto perdido!"
#define D_OBJECT_CREATE_FAILED "No se pudo crear objeto %u"
#define D_OBJECT_PAGE_UNKNOWN "La pagina ID %u no esta definida"
#define D_OBJECT_EVENT_UNKNOWN "NO se conoce el evento %d "

#define D_ATTRIBUTE_UNKNOWN "Propiedad %s desconocida"
// D_ATTRIBUTE_OBSOLETE D_ATTRIBUTE_INSTEAD can be used together or just D_ATTRIBUTE_OBSOLETE alone
#define D_ATTRIBUTE_OBSOLETE "%s esta obsoleto"
#define D_ATTRIBUTE_INSTEAD ", usa %s en su lugar"
#define D_ATTRIBUTE_READ_ONLY "%s es solo lectura"
#define D_ATTRIBUTE_PAGE_METHOD_INVALID "No se puede llamar %s en una pagina"
#define D_ATTRIBUTE_ALIGN_INVALID "Invalid align property: %s" // new
#define D_ATTRIBUTE_COLOR_INVALID "Invalid color property: %s" // new
#define D_ATTRIBUTE_LONG_MODE_INVALID "Invalid long mode: %s"  // new

#define D_OOBE_SSID_VALIDATED "SSID %s validado"
#define D_OOBE_AUTO_CALIBRATE "Auto calibracion hablitada"
#define D_OOBE_CALIBRATED "Ya se ha calibrado"

#define D_DISPATCH_COMMAND_NOT_FOUND "No se encontro el comando '%s'"
#define D_DISPATCH_INVALID_PAGE "Pagina invalida %s"
#define D_DISPATCH_REBOOT "Reiniciando microprocesador!"

#define D_JSON_FAILED "No se pudo analizar JSON:"
#define D_JSONL_FAILED "El analisis del JSONL fallo en la línea %u"
#define D_JSONL_SUCCEEDED "JSONL analizado"

#define D_OTA_CHECK_UPDATE "Buscando actualizacion en URL: %s"
#define D_OTA_CHECK_COMPLETE "Verificacion de actualizacion completa"
#define D_OTA_CHECK_FAILED "Fallo la verificacion de actualizacion: %s"
#define D_OTA_UPDATE_FIRMWARE "Actualizacion de firmware OTA"
#define D_OTA_UPDATE_COMPLETE "Actualizacion OTA completada"
#define D_OTA_UPDATE_APPLY "Aplicando el nuevo firmware y reinicio"
#define D_OTA_UPDATE_FAILED "La actualizacion OTA fallo"
#define D_OTA_UPDATING_FIRMWARE "Actualizando el firmware..."
#define D_OTA_UPDATING_FILESYSTEM "Actualizando el sistema de archivos..."

#define D_HTTP_HASP_DESIGN "Diseño de HASP"
#define D_HTTP_INFORMATION "Informacion"
#define D_HTTP_HTTP_SETTINGS "Ajustes HTTP"
#define D_HTTP_HTTP_SETTINGS "Ajustes FTP"
#define D_HTTP_WIFI_SETTINGS "Ajustes Wifi"
#define D_HTTP_GUI_SETTINGS "Ajustes WireGuard"
#define D_HTTP_MQTT_SETTINGS "Ajustes MQTT"
#define D_HTTP_GPIO_SETTINGS "Ajustes GPIO"
#define D_HTTP_MDNS_SETTINGS "Ajustes mDNS"
#define D_HTTP_TELNET_SETTINGS "Ajustes Telnet"
#define D_HTTP_DEBUG_SETTINGS "Ajustes de depuracion"
#define D_HTTP_GUI_SETTINGS "Ajustes de Pantalla"
#define D_HTTP_SAVE_SETTINGS "Guardar configuracion"
#define D_HTTP_UPLOAD_FILE "Cargar archivo"
#define D_HTTP_ERASE_DEVICE "Borrar configuracion"
#define D_HTTP_ADD_GPIO "Agragar un nuevo pin"
#define D_HTTP_BACK "Atras"
#define D_HTTP_REFRESH "Refrescar"
#define D_HTTP_PREV_PAGE "Pagina Previa"
#define D_HTTP_NEXT_PAGE "Siguiente Pagina"
#define D_HTTP_CALIBRATE "Calibrar"
#define D_HTTP_ANTIBURN "Run Anti Burn-in" // New
#define D_HTTP_SCREENSHOT "Imagen de Pantalla"
#define D_HTTP_FILE_BROWSER "Editor de Archivos"
#define D_HTTP_FIRMWARE_UPGRADE "Actualizacion de firmware"
#define D_HTTP_UPDATE_FIRMWARE "Actualizar firmware"
#define D_HTTP_FACTORY_RESET "Restaurar conf de fabrica"
#define D_HTTP_MAIN_MENU "Menu principal"
#define D_HTTP_REBOOT "Reiniciar"
#define D_HTTP_CONFIGURATION "Configuracion"
#define D_HTTP_CONFIG_CHANGED                                                                                          \
    "La configuracion ha cambiado, haga clic en <a href='/reboot'>Reiniciar</a> para guardar los cambios en la  flash"      
#define D_HTTP_SENDING_PAGE "Se envio pagina %S a %s"
#define D_HTTP_FOOTER "por Francis Van Roie"

#define D_INFO_VERSION "Version"
#define D_INFO_BUILD_DATETIME "Fecha de compilacion"
#define D_INFO_ENVIRONMENT "Environment" // new
#define D_INFO_UPTIME "Tiempo activo"
#define D_INFO_FREE_HEAP "Heap libre"
#define D_INFO_FREE_BLOCK "Bloques libres"
#define D_INFO_DEVICE_MEMORY "Memoria de dispositivo"
#define D_INFO_LVGL_MEMORY "Memoria LVGL"
#define D_INFO_TOTAL_MEMORY "Total"
#define D_INFO_FREE_MEMORY "Libre"
#define D_INFO_FRAGMENTATION "Fragmentacion"
#define D_INFO_PSRAM_FREE "PSRam libre"
#define D_INFO_PSRAM_SIZE "Tamaño PSRam "
#define D_INFO_FLASH_SIZE "Tamaño Flash"
#define D_INFO_SKETCH_USED "Memoria programa usada"
#define D_INFO_SKETCH_FREE "Memoria Programa libre"
#define D_INFO_FS_SIZE "Filesystem Size"
#define D_INFO_FS_USED "Filesystem Used"
#define D_INFO_FS_FREE "Filesystem Free"
#define D_INFO_MODULE "Modulo"
#define D_INFO_MODEL "Modelo"
#define D_INFO_FREQUENCY "Frecuencia"
#define D_INFO_CORE_VERSION "Version del nucleo"
#define D_INFO_RESET_REASON "Razon de ultimo Reset"
#define D_INFO_STATUS "Estado"
#define D_INFO_SERVER "Servidor"
#define D_INFO_USERNAME "Nombre de usuario"
#define D_INFO_CLIENTID "ID de Cliente"
// #define D_INFO_CONNECTED "Connectado"
// #define D_INFO_DISCONNECTED "Desconectado"
#define D_INFO_RECEIVED "Recivido"
#define D_INFO_PUBLISHED "Publicado"
#define D_INFO_FAILED "Fallado"
#define D_INFO_ETHERNET "Ethernet"
#define D_INFO_WIFI "Wifi"
#define D_INFO_WIREGUARD "WireGuard"
#define D_INFO_LINK_SPEED "Velocidad de enlace"
#define D_INFO_FULL_DUPLEX "Full Duplex"
#define D_INFO_BSSID "BSSID"
#define D_INFO_SSID "SSID"
#define D_INFO_RSSI "Potencia de señal"
#define D_INFO_IP_ADDRESS "Direccion IP"
#define D_INFO_MAC_ADDRESS "Direccion MAC"
#define D_INFO_GATEWAY "Gateway"
#define D_INFO_DNS_SERVER "Servidor DNS"
#define D_INFO_ENDPOINT_IP "IP de punto final"
#define D_INFO_ENDPOINT_PORT "Puerto de punto final"

#define D_OOBE_MSG "Toque la pantalla para ajustar WiFi o conectarse a un punto de acceso"
#define D_OOBE_SCAN_TO_CONNECT "Scanee para conectar"

#define D_WIFI_CONNECTING_TO "Connectando a %s"
#define D_WIFI_CONNECTED_TO "Connectado a %s, pidiendo IP..."
#define D_WIFI_RSSI_EXCELLENT "Excellente"
#define D_WIFI_RSSI_GOOD "Buena"
#define D_WIFI_RSSI_FAIR "Pasable"
#define D_WIFI_RSSI_WEAK "Debil"
#define D_WIFI_RSSI_BAD "Muy baka"

#define D_WG_INITIALIZED "Inicializado"
#define D_WG_BAD_CONFIG "Falta o mala configuración"

#define D_GPIO_SWITCH "Switch"
#define D_GPIO_BUTTON "Boton"
#define D_GPIO_TOUCH "Capacitive Touch" // new
#define D_GPIO_LED "DEL"
#define D_GPIO_LED_R "animo Red"
#define D_GPIO_LED_G "animo Green"
#define D_GPIO_LED_B "animo Blue"
#define D_GPIO_POWER_RELAY "Power Rele" // new
#define D_GPIO_LIGHT_RELAY "Light Rele" // new
#define D_GPIO_PWM "PWM"
#define D_GPIO_DAC "DAC"
#define D_GPIO_SERIAL_DIMMER "Atenuador serial"
#define D_GPIO_UNKNOWN "Desconocido"
#define D_GPIO_PIN "Pin"
#define D_GPIO_GROUP "Grupo"
#define D_GPIO_GROUP_NONE "Ninguno"
#define D_GPIO_STATE_NORMAL "Normal"     // new
#define D_GPIO_STATE_INVERTED "Inverted" // new
#define D_HTTP_SENDING_PAGE "Enviando %S pagina a %s"
#endif