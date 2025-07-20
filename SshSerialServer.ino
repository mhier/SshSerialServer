#include <libssh_esp32.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <libssh/libssh.h>
#include <libssh/server.h>

constexpr int SSH_PORT = 22;

ssh_bind sshbind = nullptr;
ssh_session session = nullptr;
ssh_channel channel = nullptr;
ssh_message message = nullptr;

WiFiServer dummyServer(SSH_PORT);  // Placeholder for Arduino compatibility

bool authenticated = false;
bool channelReady = false;

unsigned long lastPoll = 0;

/*********************************************************************************************************************/

// Helper function to check whether the incoming key of a new connection is authorised
bool isAuthorizedKey(const String& incomingKey) {
  File file = SPIFFS.open("/authorized_keys", "r");
  if (!file) return false;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line.startsWith("#")) continue;

    int firstSpace = line.indexOf(' ');
    int secondSpace = line.indexOf(' ', firstSpace + 1);
    if (firstSpace < 0) continue;
    if (secondSpace < 0) secondSpace = line.length();

    String allowedKey = line.substring(0, secondSpace);
    allowedKey.trim();

    if (incomingKey == allowedKey) {
      file.close();
      return true;
    }
  }
  file.close();
  return false;
}

/*********************************************************************************************************************/

void setup() {
  // Initialise serial interfaces
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 18, 19);

  // Initialise SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS");
    while (true) delay(100);
  }

  // Read WiFi credentials
  File file = SPIFFS.open("/wificred.txt", "r");
  if (!file) {
    Serial.println("Missing wificred.txt");
    while (true) delay(100);
  }

  String ssid = file.readStringUntil('\n');
  String pass = file.readStringUntil('\n');
  ssid.trim();
  pass.trim();

  // Connect to WiFi
  Serial.printf("Connecting to %s\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Setup SSH server
  sshbind = ssh_bind_new();
  ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT, &SSH_PORT);
  ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HOSTKEY, "ssh-rsa");
  ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_RSAKEY, "/spiffs/ssh_host_rsa_key");

  if (ssh_bind_listen(sshbind) < 0) {
    Serial.printf("Error listening: %s\n", ssh_get_error(sshbind));
  } else {
    Serial.println("SSH server listening");
  }
}

/*********************************************************************************************************************/

// Periodically called in loop(). Check and deal with incoming new connections (if no connection present)
void handleNewSSHSession() {
  if (!session) {
    session = ssh_new();
    if (ssh_bind_accept(sshbind, session) == SSH_ERROR) {
      ssh_free(session);
      session = nullptr;
      return;
    }
    ssh_handle_key_exchange(session);
    authenticated = false;
    channelReady = false;
  }

  if (!authenticated) {
    message = ssh_message_get(session);
    if (!message) return;

    if (ssh_message_type(message) == SSH_REQUEST_AUTH &&
        ssh_message_subtype(message) == SSH_AUTH_METHOD_PUBLICKEY) {
      ssh_key key = ssh_message_auth_pubkey(message);
      char* pubkey_str = nullptr;

      if (ssh_pki_export_pubkey_base64(key, &pubkey_str) == SSH_OK) {
        String incomingKey = "ssh-rsa ";
        incomingKey += String(pubkey_str);

        if (isAuthorizedKey(incomingKey)) {
          ssh_message_auth_reply_success(message, 0);
          authenticated = true;
          Serial.println("Authenticated");
        } else {
          ssh_message_reply_default(message);
        }
        ssh_string_free_char(pubkey_str);
      }
    } else {
      ssh_message_reply_default(message);
    }
    ssh_message_free(message);
    return;
  }

  if (!channelReady) {
    message = ssh_message_get(session);
    if (!message) return;

    if (ssh_message_type(message) == SSH_REQUEST_CHANNEL_OPEN &&
        ssh_message_subtype(message) == SSH_CHANNEL_SESSION) {
      channel = ssh_message_channel_request_open_reply_accept(message);
      channelReady = true;
      Serial.println("Channel session opened");
    } else {
      ssh_message_reply_default(message);
    }
    ssh_message_free(message);
  }
}

/*********************************************************************************************************************/

void handleSSHIO() {
  if (!channelReady || !channel || !session) return;

  if (ssh_channel_is_open(channel) && !ssh_channel_is_eof(channel)) {
    if (ssh_channel_poll(channel, 0) > 0) {
      char buffer[256];
      int n = ssh_channel_read(channel, buffer, sizeof(buffer), 0);
      if (n > 0) Serial1.write((uint8_t*)buffer, n);
    }

    while (Serial1.available()) {
      char c = Serial1.read();
      ssh_channel_write(channel, &c, 1);
    }
  } else {
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    ssh_disconnect(session);
    ssh_free(session);

    channel = nullptr;
    session = nullptr;
    authenticated = false;
    channelReady = false;

    Serial.println("SSH session closed");
  }
}

/*********************************************************************************************************************/

void loop() {
  handleNewSSHSession();

  if (millis() - lastPoll > 10) {
    handleSSHIO();
    lastPoll = millis();
  }
}

/*********************************************************************************************************************/
