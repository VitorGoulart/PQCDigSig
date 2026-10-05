#pragma once

namespace tcc {

// Prepare providers, algorithm implementations and liboqs once per process,
// and OpenSSL's random generators once per calling thread. Call on the thread
// that will run the measurements, before starting any timers. No keys,
// signatures or verification samples are produced here.
void initialize_crypto_runtime();

}  // namespace tcc
