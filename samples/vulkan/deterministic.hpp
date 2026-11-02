static unsigned long long deterministicState = 1;

inline time_t time(time_t *)
{
	return 0;
}

inline void srand(unsigned int seed)
{
	deterministicState = seed;
}

inline int rand()
{
	deterministicState = deterministicState * 6364136223846793005ULL + 1442695040888963407ULL;
	const double unit = double(deterministicState >> 49) / 32767.0;
	return int(unit * RAND_MAX + 0.5);
}
