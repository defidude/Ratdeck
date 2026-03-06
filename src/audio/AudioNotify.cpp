// Audio output for T-Deck Plus via I2S speaker amplifier
#include "AudioNotify.h"
#include "config/BoardConfig.h"
#include <driver/i2s.h>
#include <math.h>

#define AUDIO_SAMPLE_RATE  16000
#define I2S_PORT           I2S_NUM_0

void AudioNotify::begin() {
    i2s_config_t i2s_config = {};
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    i2s_config.sample_rate = AUDIO_SAMPLE_RATE;
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2s_config.dma_buf_count = 4;
    i2s_config.dma_buf_len = 256;
    i2s_config.use_apll = false;
    i2s_config.tx_desc_auto_clear = true;

    i2s_pin_config_t pin_config = {};
    pin_config.mck_io_num = I2S_MCLK;
    pin_config.bck_io_num = I2S_BCK;
    pin_config.ws_io_num = I2S_WS;
    pin_config.data_out_num = I2S_DOUT;
    pin_config.data_in_num = I2S_PIN_NO_CHANGE;

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[AUDIO] I2S install failed: %d\n", err);
        return;
    }

    err = i2s_set_pin(I2S_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[AUDIO] I2S pin config failed: %d\n", err);
        i2s_driver_uninstall(I2S_PORT);
        return;
    }

    i2s_zero_dma_buffer(I2S_PORT);
    _i2sReady = true;
    Serial.println("[AUDIO] I2S initialized");
}

void AudioNotify::end() {
    if (_i2sReady) {
        i2s_driver_uninstall(I2S_PORT);
        _i2sReady = false;
    }
}

void AudioNotify::writeTone(uint16_t freq, uint16_t durationMs) {
    if (!_enabled || !_i2sReady) return;

    int numSamples = (AUDIO_SAMPLE_RATE * durationMs) / 1000;
    int16_t* buf = (int16_t*)malloc(numSamples * sizeof(int16_t));
    if (!buf) return;

    float vol = (_volume / 100.0f) * 16000.0f;
    int fadeN = AUDIO_SAMPLE_RATE / 100; // 10ms fade

    for (int i = 0; i < numSamples; i++) {
        float t = (float)i / AUDIO_SAMPLE_RATE;
        // Fundamental + 2nd/3rd harmonics for warmth
        float s = sinf(2.0f * M_PI * freq * t) * 0.70f
                + sinf(2.0f * M_PI * freq * 2.0f * t) * 0.20f
                + sinf(2.0f * M_PI * freq * 3.0f * t) * 0.10f;
        // Fade envelope
        float env = 1.0f;
        if (i < fadeN) env = (float)i / fadeN;
        if (i > numSamples - fadeN) env = (float)(numSamples - i) / fadeN;
        buf[i] = (int16_t)(s * env * vol);
    }

    size_t written = 0;
    i2s_write(I2S_PORT, buf, numSamples * sizeof(int16_t), &written, portMAX_DELAY);
    free(buf);
}

void AudioNotify::writeSilence(uint16_t durationMs) {
    if (!_i2sReady) return;
    int numSamples = (AUDIO_SAMPLE_RATE * durationMs) / 1000;
    int16_t* buf = (int16_t*)calloc(numSamples, sizeof(int16_t));
    if (!buf) return;
    size_t written = 0;
    i2s_write(I2S_PORT, buf, numSamples * sizeof(int16_t), &written, portMAX_DELAY);
    free(buf);
}

void AudioNotify::playMessage() {
    if (!_enabled) return;
    writeTone(1000, 50);
    writeSilence(50);
    writeTone(1000, 50);
    writeSilence(30);
}

void AudioNotify::playAnnounce() {
    if (!_enabled) return;
    writeTone(800, 30);
    writeSilence(20);
}

void AudioNotify::playError() {
    if (!_enabled) return;
    for (int i = 0; i < 3; i++) {
        writeTone(400, 100);
        if (i < 2) writeSilence(50);
    }
    writeSilence(30);
}

void AudioNotify::playBoot() {
    if (!_enabled || !_i2sReady) return;

    // === RATDECK BOOT SEQUENCE ===
    // Sci-fi computer startup: sweep -> digital arpeggio -> confirmation
    // Total ~550ms

    const int sr = AUDIO_SAMPLE_RATE;
    const int totalMs = 560;
    const int totalSamples = sr * totalMs / 1000;

    int16_t* buf = (int16_t*)ps_malloc(totalSamples * sizeof(int16_t));
    if (!buf) {
        buf = (int16_t*)malloc(totalSamples * sizeof(int16_t));
        if (!buf) return;
    }
    memset(buf, 0, totalSamples * sizeof(int16_t));

    float vol = (_volume / 100.0f) * 16000.0f;
    int pos = 0;

    // Helper: add a tone with harmonics at current position
    auto addTone = [&](float freq, int ms) {
        int n = sr * ms / 1000;
        int fadeN = sr * 8 / 1000; // 8ms fade
        for (int i = 0; i < n && (pos + i) < totalSamples; i++) {
            float t = (float)i / sr;
            float s = sinf(2.0f * M_PI * freq * t) * 0.65f
                    + sinf(2.0f * M_PI * freq * 2.0f * t) * 0.22f
                    + sinf(2.0f * M_PI * freq * 3.0f * t) * 0.13f;
            float env = 1.0f;
            if (i < fadeN) env = (float)i / fadeN;
            if (i > n - fadeN) env = (float)(n - i) / fadeN;
            buf[pos + i] = (int16_t)(s * env * vol);
        }
        pos += n;
    };

    // Helper: frequency sweep with harmonics
    auto addSweep = [&](float startF, float endF, int ms) {
        int n = sr * ms / 1000;
        int fadeN = sr * 8 / 1000;
        float phase = 0;
        for (int i = 0; i < n && (pos + i) < totalSamples; i++) {
            float t = (float)i / n; // 0..1 progress
            float freq = startF + (endF - startF) * t * t; // quadratic sweep (accelerating)
            phase += 2.0f * M_PI * freq / sr;
            float s = sinf(phase) * 0.65f
                    + sinf(phase * 2.0f) * 0.22f
                    + sinf(phase * 3.0f) * 0.08f;
            float env = 1.0f;
            if (i < fadeN) env = (float)i / fadeN;
            if (i > n - fadeN) env = (float)(n - i) / fadeN;
            buf[pos + i] = (int16_t)(s * env * vol);
        }
        pos += n;
    };

    auto addSilence = [&](int ms) {
        pos += sr * ms / 1000;
    };

    // Phase 1: Rising power sweep 300->1200Hz (160ms) — "systems powering up"
    addSweep(300, 1200, 160);
    addSilence(25);

    // Phase 2: Three quick ascending staccato notes — E5, G#5, B5
    // (E major triad in 2nd inversion — bright, triumphant, slightly edgy)
    addTone(659,  45);   // E5
    addSilence(12);
    addTone(831,  45);   // G#5
    addSilence(12);
    addTone(988,  45);   // B5
    addSilence(25);

    // Phase 3: Descending glitch sweep 2400->1600Hz (60ms) — "digital handshake"
    addSweep(2400, 1600, 60);
    addSilence(20);

    // Phase 4: Final confirmation — E6 (1319Hz), 100ms with clean decay — "online"
    addTone(1319, 100);

    // Write entire sequence at once for seamless playback
    size_t written = 0;
    i2s_write(I2S_PORT, buf, pos * sizeof(int16_t), &written, portMAX_DELAY);

    // Flush with silence
    memset(buf, 0, 512 * sizeof(int16_t));
    i2s_write(I2S_PORT, buf, 512 * sizeof(int16_t), &written, portMAX_DELAY);

    free(buf);
}
