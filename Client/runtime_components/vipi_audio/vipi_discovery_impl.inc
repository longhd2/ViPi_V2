void ViPiAudio::send_discovery_() {
  if (!network::is_connected()) return;

  int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (s < 0) {
    ESP_LOGW(TAG, "SERVER HANDSHAKE: socket failed");
    return;
  }

  sockaddr_in dst{};
  dst.sin_family = AF_INET;
  dst.sin_port = htons(12345);
  if (inet_pton(AF_INET, server_ip_.c_str(), &dst.sin_addr) != 1) {
    ESP_LOGW(TAG, "SERVER HANDSHAKE: bad server IP %s", server_ip_.c_str());
    close(s);
    return;
  }

  const char *msg = "vipi-v2";
  int sent = sendto(s, msg, strlen(msg), 0, (sockaddr *) &dst, sizeof(dst));
  close(s);

  if (sent == (int) strlen(msg)) {
    discovery_sent_ = true;
    last_discovery_ms_ = millis();
    ESP_LOGI(TAG, "SERVER HANDSHAKE SENT -> %s:12345 [%s]",
             server_ip_.c_str(), msg);
  } else {
    ESP_LOGW(TAG, "SERVER HANDSHAKE SEND FAILED -> %s:12345", server_ip_.c_str());
  }
}

