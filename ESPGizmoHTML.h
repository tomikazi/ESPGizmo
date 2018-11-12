

#define NET_HTML \
"<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
    <title>Network Setup</title>\
    <style>\
        body {margin:10px;padding:10px;color:#000000;background-color:#ffffff;font-family:Arial,Helvetica,Sans-Serif;font-size:1.4em;}\
        h1 {color:#AA0000;margin-top:4px;width:100%;}\
        h3 {color:#080808;font-size:1.1em;margin-bottom:4px;}\
        input {border:1px solid #2f2f2f;border-radius:4px;font-size:0.9em;padding-left:8px;}\
        input[type=submit] {border-radius:4px;background-color:#f0f0f0;font-size:1.3em;}\
        select {border:1px solid #2f2f2f;border-radius:4px;font-size:0.9em;}\
    </style>\
</head>\
<body><h1>Network Setup</h1>\
<form action=\"/netcfg\">\
    <h3>Name</h3><input type=\"text\" name=\"name\" value=\"%s\" size=\"30\">\
    <h3>Network</h3><select id=\"netlist\" onchange='document.getElementById(\"net\").value = document.getElementById(\"netlist\").value'>\%s</select><p>\
    <input type=\"text\" id=\"net\" name=\"net\" value=\"%s\" size=\"30\">\
    <h3>Password</h3><input type=\"password\" name=\"pass\" value=\"%s\" size=\"30\"><p>\
    <h3>IP Address</h3>%s<p>\
    <input type=\"submit\" value=\"Apply Changes\">\
</form>\
</body>\
</html>"

#define NETCFG_HTML \
"<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
    <meta http-equiv=\"refresh\" content=\"5;url=/nets\">\
    <title>Network Reconfigured</title>\
    <style>\
        body {margin:10px;padding:10px;color:#000000;background-color:#ffffff;font-family:Arial,Helvetica,Sans-Serif;font-size:1.4em;}\
        h1 {color:#AA0000;margin-top:4px;width:100%;}\
    </style>\
</head>\
<body><h1>Network Reconfigured</h1>\
    Reconfigured for connection to %s. Restarting...\
</body>\
</html>"

#define MQTT_HTML \
"<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
    <title>MQTT Setup</title>\
    <style>\
        body {margin:10px;padding:10px;color:#000000;background-color:#ffffff;font-family:Arial,Helvetica,Sans-Serif;font-size:1.4em;}\
        h1 {color:#AA0000;margin-top:4px;width:100%;}\
        h3 {color:#080808;font-size:1.1em;margin-bottom:4px;}\
        input {border:1px solid #2f2f2f;border-radius:4px;font-size:0.9em;padding-left:8px;}\
        input[type=submit] {border-radius:4px;background-color:#f0f0f0;font-size:1.3em;}\
        select {border:1px solid #2f2f2f;border-radius:4px;font-size:0.9em;}\
    </style>\
</head>\
<body><h1>MQTT Setup</h1>\
<form action=\"/mqttcfg\">\
    <h3>MQTT Host</h3><input type=\"text\" name=\"host\" value=\"%s\" size=\"30\">\
    <h3>MQTT Port</h3><input type=\"text\" name=\"port\" value=\"%d\" size=\"30\"><p>\
    <h3>User Name</h3><input type=\"password\" name=\"user\" value=\"%s\" size=\"30\"><p>\
    <h3>Password</h3><input type=\"password\" name=\"pass\" value=\"%s\" size=\"30\"><p>\
    <h3>Topic Prefix</h3><input type=\"text\" name=\"prefix\" value=\"%s\" size=\"30\">\
    <input type=\"submit\" value=\"Apply Changes\">\
</form>\
</body>\
</html>"

#define MQTTCFG_HTML \
"<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
    <meta http-equiv=\"refresh\" content=\"5;url=/nets\">\
    <title>MQTT Reconfigured</title>\
    <style>\
        body {margin:10px;padding:10px;color:#000000;background-color:#ffffff;font-family:Arial,Helvetica,Sans-Serif;font-size:1.4em;}\
        h1 {color:#AA0000;margin-top:4px;width:100%;}\
    </style>\
</head>\
<body><h1>MQTT Reconfigured</h1>\
    Reconfigured for connection to %s. Restarting...\
</body>\
</html>"

#define UPDATE_HTML \
"<!DOCTYPE html>\
<html lang=\"en\">\
<head>\
    <meta http-equiv=\"refresh\" content=\"5;url=/\">\
    <title>Update Requested</title>\
    <style>\
        body {margin:10px;padding:10px;color:#000000;background-color:#ffffff;font-family:Arial,Helvetica,Sans-Serif;font-size:1.4em;}\
        h1 {color:#AA0000;margin-top:4px;width:100%;}\
    </style>\
</head>\
<body><h1>Updated Requested</h1>\
    Scheduled update from URL %s; current version is %s.\
</body>\
</html>"