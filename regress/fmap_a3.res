{
	songfilt f {
		filt {
			evmap any {7 4..15} > any {3 0..11}
			evmap any {7 9} > any {3 8}
			evmap any {7 9} > any {0 1}
		}
	}
	curfilt f
}
