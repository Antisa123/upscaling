// Shared helpers for the metric passes. Everything is scored in display space
// by default, which is the domain PSNR/SSIM numbers are conventionally quoted
// in; comparing raw linear HDR values would let highlights dominate the error.

vec3 metricsToDisplay(vec3 c, int tonemap) {
    if (tonemap == 0) return c;
    c = max(c, vec3(0.0));
    return pow(c / (1.0 + c), vec3(1.0 / 2.2));
}

float metricsLuma(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}
