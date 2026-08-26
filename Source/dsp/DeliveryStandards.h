#pragma once

namespace cinelab
{

// ============================================================================
// Delivery standards: the "legal number" for each target.
// Values follow the public industry specs (EBU R128, Netflix, YouTube, etc.).
// ============================================================================
enum class Destination
{
    cinema   = 0, // DCP — EBU R128 / SMPTE ST 202
    tv       = 1, // broadcast — EBU R128 / ATSC A/85
    netflix  = 2, // Netflix spec: −27 LKFS, peak ≤ −2 dBTP
    web      = 3, // YouTube/Web: −14 LUFS, peak ≤ −1 dBTP
    podcast  = 4, // typical podcast
    manual   = 5  // user-defined target/ceiling
};

struct DeliveryStandard
{
    const char* name;        // short name
    const char* humanName;   // readable name
    double      targetLufs;  // loudness target
    double      ceilingDb;   // peak ceiling (dBFS ≈ dBTP)
    const char* blurb;       // plain-language description
};

inline const DeliveryStandard& deliveryStandardFor (Destination d)
{
    static const DeliveryStandard standards[] = {
        { "Cinema",    "Cinema (room / DCP)", -24.0, -3.0,
          "Standardized cinema sound for large rooms. The reference for feature films." },
        { "TV",        "TV / Broadcast",      -23.0, -2.0,
          "European TV broadcast standard (EBU R128). Consistent level on any channel." },
        { "Netflix",   "Netflix",             -27.0, -2.0,
          "Netflix spec: calm loudness, huge dynamic range." },
        { "Web",       "YouTube / Web",       -14.0, -1.0,
          "Loud and even: what the web requires so you are not crushed by other videos." },
        { "Podcast",   "Podcast",             -16.0, -1.5,
          "Clear, steady voice for headphones or the car." },
        { "Manual",    "Manual",              -24.0, -1.0,
          "You set the target and the ceiling with the Pro controls." },
    };

    return standards[(int) d];
}

} // namespace cinelab