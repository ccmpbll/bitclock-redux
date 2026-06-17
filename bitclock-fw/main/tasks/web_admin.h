#pragma once

// Persistent HTTP admin server, runs in STA mode once Wi-Fi is up.
// Reachable at the device IP or http://bitclock.local
void web_admin_start();
void web_admin_stop();
