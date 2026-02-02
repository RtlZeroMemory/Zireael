//go:build !windows

package main

type xorshift32 struct {
	s uint32
}

func (r *xorshift32) Seed(v uint32) {
	if v == 0 {
		v = 0x12345678
	}
	r.s = v
}

func (r *xorshift32) Next() uint32 {
	x := r.s
	x ^= x << 13
	x ^= x >> 17
	x ^= x << 5
	r.s = x
	return x
}
