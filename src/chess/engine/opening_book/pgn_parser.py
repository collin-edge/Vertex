# TODO: FINISH

import struct

class MT19937_64:
    # Python's random module uses the 32-bit version of MT19937, so we gotta make the 64 bit version ourself
    # i got all the math from https://github.com/mattgallagher/CwlUtils/blob/master/Sources/ReferenceRandomGenerators/mt19937-64.c
    # whole bunch of bullshit

    NN = 312
    MM = 156

    F = 6364136223846793005

    MASK = 0xFFFFFFFFFFFFFFFF

    MATRIX_A = 0xB5026F5AA96619E9
    UM = 0xFFFFFFFF80000000
    LM = 0x7FFFFFFF

    def __init__(self, seed):
        self.MT = [0] * MT19937_64.NN
        self.MT[0] = seed & MT19937_64.MASK
        self.idx = MT19937_64.NN

        for i in range(1, MT19937_64.NN):
            self.MT[i] = (MT19937_64.F * (self.MT[i - 1] ^ (self.MT[i - 1] >> 62)) + i) & MT19937_64.MASK # python integers are annoying and can just grow indefinitely

    def twist(self):
        for i in range(MT19937_64.NN - MT19937_64.MM):
            x = (self.MT[i] & MT19937_64.UM) | (self.MT[i + 1] & MT19937_64.LM)

            xa = 0
            if x & 1: xa = (x >> 1) ^ MT19937_64.MATRIX_A
            else: xa = x >> 1

            self.MT[i] = self.MT[i + MT19937_64.MM] ^ xa

        for i in range((MT19937_64.NN - MT19937_64.MM), MT19937_64.NN - 1):
            x = (self.MT[i] & MT19937_64.UM) | (self.MT[i + 1] & MT19937_64.LM)

            xa = 0
            if x & 1: xa = (x >> 1) ^ MT19937_64.MATRIX_A
            else: xa = x >> 1

            self.MT[i] = self.MT[i + MT19937_64.MM - MT19937_64.NN] ^ xa

        x = (self.MT[MT19937_64.NN - 1] & MT19937_64.UM) | (self.MT[0] & MT19937_64.LM)

        xa = 0
        if x & 1: xa = (x >> 1) ^ MT19937_64.MATRIX_A
        else: xa = x >> 1

        self.MT[MT19937_64.NN - 1] = self.MT[MT19937_64.MM - 1] ^ xa

        self.idx = 0

    def __call__(self):
        if (self.idx >= MT19937_64.NN): self.twist()

        x = self.MT[self.idx]
        self.idx += 1

        x ^= (x >> 29) & 0x5555555555555555
        x ^= (x << 17) & 0x71D67FFFEDA60000
        x ^= (x << 37) & 0xFFF7EEE000000000
        x ^= x >> 43

        return x

rng = MT19937_64(0xC0ED6E)

PIECE_KEYS = [[0 for j in range(64)] for i in range(12)]
for i in range(12):
    for j in range (64):
        PIECE_KEYS[i][j] = rng()

SIDE_TO_MOVE = rng()

CASTLING_RIGHTS = [0] * 16
for i in range(16):
    CASTLING_RIGHTS[i] = rng()

EN_PASSANT_FILE = [0] * 8
for i in range(8):
    EN_PASSANT_FILE[i] = rng()