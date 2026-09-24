// Included inside a sample's namespace, ahead of its source: its unqualified
// time(), srand() and rand() find these before the C library's. The samples
// seed from the clock, and the host's and the guest's C libraries do not share
// a generator or a RAND_MAX, so without these the two pictures of a sample
// scattering particles or instances could never match.

static unsigned long long deterministicState = 1;

inline time_t time(time_t *)
{
	return 0;
}

inline void srand(unsigned int seed)
{
	deterministicState = seed;
}

// Fifteen bits from a 64-bit LCG, scaled to this C library's RAND_MAX, so
// rand() / RAND_MAX is the same fraction on every machine.
inline int rand()
{
	deterministicState = deterministicState * 6364136223846793005ULL + 1442695040888963407ULL;
	const double unit = double(deterministicState >> 49) / 32767.0;
	return int(unit * RAND_MAX + 0.5);
}
