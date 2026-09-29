"""Exercise the real loop with a Wi-Fi start delay and a simulated clock."""

from pathlib import Path
import subprocess
import tempfile
source = (Path(__file__).resolve().parents[1] / 'wifi_setup.cpp').read_text()
loop = source[source.index('void wifiSetupLoop() {'):]
preamble = r'''
#include <cstdint>
#include <string>
#include <cstdio>
using String = std::string;
enum class State { setup, pending, connecting, connected };
State state = State::pending;
uint32_t clockMs = 1000, stateSince = 0, connectedSince = 0;
constexpr uint32_t connectTimeoutMs = 30000, setupGraceMs = 60000;
constexpr int WIFI_SCAN_RUNNING = -1, WL_CONNECTED = 3, WIFI_STA = 1;
bool scanning=false, saveOnConnect=false, closeSetup=false, setupActive=true, storageReady=true;
int scanCount=-1;
String notice, serialCommand;
struct Credentials { char ssid[33]; char password[65]; } credentials{};
struct Server { void handleClient() {} } server;
struct SerialType {
 int available() { return 0; } char read() { return 0; }
 template<class... T> void printf(T...) {} template<class T> void print(T) {}
 template<class T> void println(T) {}
} Serial;
struct Wifi {
 int scanComplete() { return 0; } int status() { return 0; }
 void disconnect() {} int localIP() { return 0; }
 void softAPdisconnect(bool) {} void mode(int) {}
} WiFi;
struct Preferences { size_t putBytes(const char*, void*, size_t n) { return n; } } preferences;
uint32_t millis() { return clockMs; }
void serviceResetButton(uint32_t) {}
void reopenSetup() {}
void startSetup() {}
String connectionFailure() { return "timeout"; }
void startConnection() { clockMs += 20; state = State::connecting; stateSince = millis(); }
'''
main = r'''
int main() {
 wifiSetupLoop();
 if (state != State::connecting) { puts("FAIL: new connection timed out in the same loop iteration"); return 1; }
 clockMs = stateSince + 29999;
 wifiSetupLoop();
 if (state != State::connecting) { puts("FAIL: timed out before 30 seconds"); return 1; }
 clockMs = stateSince + 30000;
 wifiSetupLoop();
 if (state != State::setup) { puts("FAIL: did not time out at 30 seconds"); return 1; }
 puts("PASS: connection survives startup and times out at 30 seconds");
}
'''
with tempfile.TemporaryDirectory(prefix="wifi-timer-") as directory:
    source_path = Path(directory) / "check.cpp"
    executable = Path(directory) / "check"
    source_path.write_text(preamble + loop + main)
    subprocess.run(["c++", "-std=c++17", str(source_path), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
