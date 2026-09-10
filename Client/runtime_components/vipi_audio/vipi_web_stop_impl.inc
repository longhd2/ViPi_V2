void ViPiAudio::stop_web_() { if(web_server_){httpd_stop(web_server_);web_server_=nullptr;} web_started_=false; }

