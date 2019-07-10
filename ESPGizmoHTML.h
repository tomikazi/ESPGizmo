
#define HTML_HEAD       "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>"
#define HTML_TITLE_END  "</title>"

#define HTML_REDIRECT_START "<meta http-equiv=\"refresh\" content=\"5;url="
#define HTML_REDIRECT_LONG_START "<meta http-equiv=\"refresh\" content=\"30;url="
#define HTML_REDIRECT_END "\">"

#define HTML_CSS_MENU \
"<style>body {margin:0;padding:0;font-family:Arial,Helvetica,Sans-Serif,serif;font-size:1.1em;}\
.hdr {padding:2px 16px;color:#ffffff;background-color:#333333;width:100%;}\
.menuBtn {position:absolute;right:16px;top:8px;width:64px;text-align:right;font-size:2em;}\
.menu {position:absolute;right:0px;top:52px;padding:0px 4px;background-color:#333333;z-index:10;font-size:1.4em;}\
.menu a {margin:0px;color:#ffffff;text-decoration:none;}\
.menu p {margin:0px;padding:6px 16px;width:128px;border-top:solid 1px #dddddd;}\
.main {padding:2px 16px;color:#000000;background-color:#ffffff;width:100%;}\
h1 {margin:4px 0;}\
h3 {color:#080808;font-size:1.0em;margin-bottom:4px;}\
input {border:1px solid #2f2f2f;border-radius:4px;font-size:1.0em;padding-left:8px;}\
input[type=submit] {border-radius:4px;background-color:#f0f0f0;font-size:1.0em;}\
select {border:1px solid #2f2f2f;border-radius:4px;font-size:1.0em;}\
</style>\
<script>\
function menu() { let m = document.getElementById(\"menu\"); m.hidden = !m.hidden; }\
</script>"

#define HTML_BODY   "</head><body><div class=\"hdr\"><h1>"
#define HTML_MENU   "</h1><div class=\"menuBtn\" onclick=\"menu()\">=</div></div>\
<div class=\"menu\" id=\"menu\" hidden=\"true\">\
<a href=\"/\"><p>Home</p></a>\
<a href=\"nets\"><p>Network</p></a>\
<a href=\"mqtt\"><p>MQTT</p></a>\
<a href=\"update\"><p>Update</p></a>\
</div>\
<div class=\"main\">"

#define HTML_END    "</div></body></html>\n\n"

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
