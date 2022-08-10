#include "GowReplayer.h"

int main(int argc, char* argv[])
{
	GowReplayer replayer;

	replayer.replay(argv[1]);

	return 0;
}