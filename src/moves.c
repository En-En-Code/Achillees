#include "board.h"
#include "play.h"

enum {NORT, NOEA, EAST, SOEA, SOUT, SOWE, WEST, NOWE};


static int pseudoLegalMoves(Board board, Move *moves);

static void pawnPseudoLegalMoves  (Board board, Move *moves, int *n);
static void knightPseudoLegalMoves(Board board, Move *moves, int *n);
static void kingPseudoLegalMoves  (Board board, Move *moves, int *n);

static void slidingPiecePseudoLegalMoves(Board board, Move *moves, int *n, uint64_t (*movesFunc)(int, uint64_t, uint64_t), int piece);

static void pawnMoves(Move *moves, int *n, uint64_t bb, int color, int shift);

static uint64_t knightMoves(int index, uint64_t myPieces);
static uint64_t kingMoves  (int index, uint64_t myPieces);

static uint64_t bishopMoves(int index, uint64_t occupied, uint64_t myPieces);
static uint64_t rookMoves  (int index, uint64_t occupied, uint64_t myPieces);
static uint64_t queenMoves (int index, uint64_t occupied, uint64_t myPieces);

static uint64_t rayAttacks(int index, uint64_t occupied, uint64_t myPieces, int bitScan(uint64_t), int dir);


int legalMoves(Board board, Move *moves) {
	Move psLegalMoves[218];
	History history;

	int kingColor = board.turn, n = 0;
	int k = pseudoLegalMoves(board, psLegalMoves);

	for (int i = 0; i < k; i++) {
		makeMove(&board, psLegalMoves[i], &history);

		if (superPieceAttacks(board, board.pieces[kingColor][KING], kingColor) == 0) {
			moves[n++] = psLegalMoves[i];
		}

		undoMove(&board, psLegalMoves[i], history);
	}

	return n;
}

static int pseudoLegalMoves(Board board, Move *moves) {
	int n = 0;

	pawnPseudoLegalMoves  (board, moves, &n);
	knightPseudoLegalMoves(board, moves, &n);
	kingPseudoLegalMoves  (board, moves, &n);

	slidingPiecePseudoLegalMoves(board, moves, &n, bishopMoves, BISHOP);
	slidingPiecePseudoLegalMoves(board, moves, &n, rookMoves, ROOK);
	slidingPiecePseudoLegalMoves(board, moves, &n, queenMoves, QUEEN);

	return n;
}

/*
 * Attacks from a certain position as if it was any piece.
 */
uint64_t superPieceAttacks(Board board, uint64_t kingBB, int color) {
	const int opColor = 1 - color, kingIndex = bitScanForward(kingBB);
	uint64_t attacked = 0;

	uint64_t bishopsAndQueens = board.pieces[opColor][BISHOP] | board.pieces[opColor][QUEEN];
	uint64_t rooksAndQueens   = board.pieces[opColor][ROOK]   | board.pieces[opColor][QUEEN];

	if (color == WHITE) {
		attacked |= (noEaOne(kingBB) | noWeOne(kingBB)) & board.pieces[opColor][PAWN];
	} else {
		attacked |= (soEaOne(kingBB) | soWeOne(kingBB)) & board.pieces[opColor][PAWN];
	}

	attacked |= knightMoves(kingIndex, board.players[color]) & board.pieces[opColor][KNIGHT];
	attacked |= kingMoves  (kingIndex, board.players[color]) & board.pieces[opColor][KING];

	attacked |= bishopMoves(kingIndex, board.occupied, board.players[color]) & bishopsAndQueens;
	attacked |= rookMoves  (kingIndex, board.occupied, board.players[color]) & rooksAndQueens;

	return attacked;
}

// PAWN

static inline uint64_t wSinglePushPawn(uint64_t bb, uint64_t empty)    { return nortOne(bb) & empty; }
static inline uint64_t bSinglePushPawn(uint64_t bb, uint64_t empty)    { return soutOne(bb) & empty; }

static uint64_t wDoublePushPawn(uint64_t bb, uint64_t empty) {
	static const uint64_t rank4 = 0x00000000FF000000;
	return wSinglePushPawn(bb, empty) & rank4;
}

static uint64_t bDoublePushPawn(uint64_t bb, uint64_t empty) {
	static const uint64_t rank5 = 0x000000FF00000000;
	return bSinglePushPawn(bb, empty) & rank5;
}

static inline uint64_t wCaptRightPawn (uint64_t bb, uint64_t opPieces) { return noEaOne(bb) & opPieces; }
static inline uint64_t bCaptRightPawn (uint64_t bb, uint64_t opPieces) { return soEaOne(bb) & opPieces; }
static inline uint64_t wCaptLeftPawn  (uint64_t bb, uint64_t opPieces) { return noWeOne(bb) & opPieces; }
static inline uint64_t bCaptLeftPawn  (uint64_t bb, uint64_t opPieces) { return soWeOne(bb) & opPieces; }

static void pawnPseudoLegalMoves(Board board, Move *moves, int *n) {
	const int color = board.turn;

	uint64_t opPieces = board.players[1 - color];
	uint64_t bb = board.pieces[color][PAWN];
	uint64_t singlePush;

	/* Capturing enPassant is not detectable in the move itself,
	 * so it has to be calculated on the fly.
	 *
	 * The condition is necessary as board.enPassant defaults to 0,
	 * which is not a valid en passant move.
	 */
	if (board.enPassant)
		opPieces |= pow2[board.enPassant];

	if (color == WHITE) {
		singlePush = wSinglePushPawn(bb, board.empty);
		pawnMoves(moves, n, singlePush, color, 8);
		pawnMoves(moves, n, wDoublePushPawn(singlePush, board.empty), color, 16);
		pawnMoves(moves, n, wCaptRightPawn(bb, opPieces), color, 9);
		pawnMoves(moves, n, wCaptLeftPawn (bb, opPieces), color, 7);
	} else {
		singlePush = bSinglePushPawn(bb, board.empty);
		pawnMoves(moves, n, singlePush, color, -8);
		pawnMoves(moves, n, bDoublePushPawn(singlePush, board.empty), color, -16);
		pawnMoves(moves, n, bCaptRightPawn(bb, opPieces), color, -7);
		pawnMoves(moves, n, bCaptLeftPawn (bb, opPieces), color, -9);
	}
}

// A possible optimization would involve making a new function for double-pushed pawns as they don't get promoted
static void pawnMoves(Move *moves, int *n, uint64_t bb, int color, int shift) {
	// Lookup table for promotions depending on color
	static const uint64_t promote[2] = {0xff00000000000000, 0xff};
	int from, to;

	// Splits the bitboard into promoting pawns and non-promoting pawns
	uint64_t promoting = bb & promote[color];
	bb ^= promoting;

	// Adds the moves for all non-promoting pawns
	if (bb) do {
		to = bitScanForward(bb);
		from = to - shift;

		moves[(*n)++] = (Move){.from=from, .to=to, .piece=PAWN, .color=color, .promotion=0};
	} while (unsetLSB(bb));

	// Adds the moves for all promoting pawns
	if (promoting) do {
		to = bitScanForward(promoting);
		from = to - shift;

		// A different move is considered for every possible promotion
		for (int piece = KNIGHT; piece <= QUEEN; piece++)
			moves[(*n)++] = (Move){.from=from, .to=to, .piece=PAWN, .color=color, .promotion=piece};
	} while (unsetLSB(promoting));
}

// KNIGHT

static void knightPseudoLegalMoves(Board board, Move *moves, int *n) {
	uint64_t movesBB, bb = board.pieces[board.turn][KNIGHT];
	int from, to;

	if (bb) do {
		from = bitScanForward(bb);
		movesBB = knightMoves(from, board.players[board.turn]);

		if (movesBB) do {
			to = bitScanForward(movesBB);
			moves[(*n)++] = (Move){.from=from, .to=to, .piece=KNIGHT, .color=board.turn};
		} while (unsetLSB(movesBB));
	} while (unsetLSB(bb));
}

// A possible optimization is to remove this function and make the lookup table a global
static uint64_t knightMoves(int index, uint64_t myPieces) {
	static uint64_t knightLookup[64] = {
			0x20400, 0x50800, 0xa1100, 0x142200, 0x284400, 0x508800, 0xa01000, 0x402000, 0x2040004, 0x5080008, 0xa110011, 0x14220022, 0x28440044, 0x50880088, 0xa0100010, 0x40200020, 0x204000402, 0x508000805, 0xa1100110a, 0x1422002214, 0x2844004428, 0x5088008850, 0xa0100010a0, 0x4020002040, 0x20400040200, 0x50800080500, 0xa1100110a00, 0x142200221400, 0x284400442800, 0x508800885000, 0xa0100010a000, 0x402000204000,0x2040004020000, 0x5080008050000, 0xa1100110a0000, 0x14220022140000, 0x28440044280000, 0x50880088500000, 0xa0100010a00000, 0x40200020400000, 0x204000402000000, 0x508000805000000, 0xa1100110a000000, 0x1422002214000000, 0x2844004428000000, 0x5088008850000000, 0xa0100010a0000000, 0x4020002040000000, 0x400040200000000, 0x800080500000000, 0x1100110a00000000, 0x2200221400000000, 0x4400442800000000,0x8800885000000000, 0x100010a000000000, 0x2000204000000000, 0x4020000000000, 0x8050000000000, 0x110a0000000000, 0x22140000000000, 0x44280000000000, 0x88500000000000, 0x10a00000000000, 0x20400000000000
	};

	return knightLookup[index] & ~myPieces;
}

// KING

static void kingPseudoLegalMoves(Board board, Move *moves, int *n) {
	static const uint64_t freeSqrsToCastle[4] = {0x60, 0xe, 0x6000000000000000, 0xe00000000000000};
	static const uint64_t legalToCastle[4] = {0x20, 0x8, 0x2000000000000000, 0x800000000000000};

	int to, from, color = board.turn;
	int kingCastle, queenCastle, castle;

	uint64_t movesBB, bb = board.pieces[color][KING];

	from = bitScanForward(bb);

	// Normal king moves
	movesBB = kingMoves(from, board.players[color]);

	if (movesBB) do {
		to = bitScanForward(movesBB);
		moves[(*n)++] = (Move){.from=from, .to=to, .piece=KING, .color=color, .castle=-1};
	} while (unsetLSB(movesBB));

	// Castling

	/* Makes sure that:
	 * 		- The king is not in check.
	 * 		- The castle to perform is enabled.
	 * 		- The squares it goes through are empty.
	 * 		- The squares the king passes by are free.
	 */

	if (superPieceAttacks(board, bb, color) == 0) {
		kingCastle = 2*color, queenCastle = kingCastle + 1;

		castle = board.castling & pow2[kingCastle];
		if (castle && ((board.empty & freeSqrsToCastle[kingCastle]) == freeSqrsToCastle[kingCastle]) &&
				(superPieceAttacks(board, legalToCastle[kingCastle], color) == 0))
		{
			moves[(*n)++] = (Move){.from=from, .to=from + 2, .piece=KING, .color=color, .castle=castle};
		}

		castle = board.castling & pow2[queenCastle];
		if (castle && ((board.empty & freeSqrsToCastle[queenCastle]) == freeSqrsToCastle[queenCastle]) &&
				(superPieceAttacks(board, legalToCastle[queenCastle], color) == 0))
		{
			moves[(*n)++] = (Move){.from=from, .to=from - 2, .piece=KING, .color=color, .castle=castle};
		}
	}
}

static uint64_t kingMoves(int index, uint64_t myPieces) {
	static const uint64_t kingLookup[64] = {
		0x302, 0x705, 0xe0a, 0x1c14, 0x3828, 0x7050, 0xe0a0, 0xc040, 0x30203, 0x70507, 0xe0a0e, 0x1c141c, 0x382838, 0x705070, 0xe0a0e0, 0xc040c0, 0x3020300, 0x7050700, 0xe0a0e00, 0x1c141c00, 0x38283800, 0x70507000, 0xe0a0e000, 0xc040c000, 0x302030000, 0x705070000, 0xe0a0e0000, 0x1c141c0000, 0x3828380000, 0x7050700000, 0xe0a0e00000, 0xc040c00000, 0x30203000000, 0x70507000000, 0xe0a0e000000, 0x1c141c000000,0x382838000000, 0x705070000000, 0xe0a0e0000000, 0xc040c0000000, 0x3020300000000, 0x7050700000000, 0xe0a0e00000000, 0x1c141c00000000, 0x38283800000000, 0x70507000000000, 0xe0a0e000000000, 0xc040c000000000, 0x302030000000000, 0x705070000000000, 0xe0a0e0000000000, 0x1c141c0000000000, 0x3828380000000000, 0x7050700000000000, 0xe0a0e00000000000, 0xc040c00000000000, 0x203000000000000, 0x507000000000000,0xa0e000000000000, 0x141c000000000000, 0x2838000000000000, 0x5070000000000000, 0xa0e0000000000000, 0x40c0000000000000
	};

	return kingLookup[index] & ~myPieces;
}

// SLIDING PIECES
static void slidingPiecePseudoLegalMoves(Board board, Move *moves, int *n, uint64_t (*movesFunc)(int, uint64_t, uint64_t), int piece) {
	const int color = board.turn;
	int from, to;

	uint64_t movesBB, bb = board.pieces[color][piece];

	if (bb) do {
		from = bitScanForward(bb);
		movesBB = movesFunc(from, board.occupied, board.players[color]);

		if (movesBB) do {
			to = bitScanForward(movesBB);
			moves[(*n)++] = (Move){.from=from, .to=to, .piece=piece, .color=color};
		} while (unsetLSB(movesBB));
	} while (unsetLSB(bb));
}

static uint64_t bishopMoves(int index, uint64_t occupied, uint64_t myPieces) {
	return  rayAttacks(index, occupied, myPieces, bitScanForward, NOEA) |
			rayAttacks(index, occupied, myPieces, bitScanForward, NOWE) |
			rayAttacks(index, occupied, myPieces, bitScanReverse, SOEA) |
			rayAttacks(index, occupied, myPieces, bitScanReverse, SOWE);
}

static uint64_t rookMoves(int index, uint64_t occupied, uint64_t myPieces) {
	return  rayAttacks(index, occupied, myPieces, bitScanForward, NORT) |
			rayAttacks(index, occupied, myPieces, bitScanForward, EAST) |
			rayAttacks(index, occupied, myPieces, bitScanReverse, SOUT) |
			rayAttacks(index, occupied, myPieces, bitScanReverse, WEST);
}

static uint64_t queenMoves(int index, uint64_t occupied, uint64_t myPieces) {
	return bishopMoves(index, occupied, myPieces) | rookMoves(index, occupied, myPieces);
}

static uint64_t rayAttacks(int index, uint64_t occupied, uint64_t myPieces, int bitScan(uint64_t), int dir) {
	// Lookup table for all eight ray-directions: NORT, NOEA, EAST, SOEA, SOUT, SOWE, WEST, NOWE
	static const uint64_t rayLookup[8][64] = {
		{0x101010101010100, 0x202020202020200, 0x404040404040400, 0x808080808080800, 0x1010101010101000, 0x2020202020202000, 0x4040404040404000, 0x8080808080808000, 0x101010101010000, 0x202020202020000, 0x404040404040000, 0x808080808080000, 0x1010101010100000, 0x2020202020200000, 0x4040404040400000, 0x8080808080800000, 0x101010101000000, 0x202020202000000, 0x404040404000000, 0x808080808000000, 0x1010101010000000, 0x2020202020000000, 0x4040404040000000, 0x8080808080000000, 0x101010100000000, 0x202020200000000, 0x404040400000000, 0x808080800000000, 0x1010101000000000, 0x2020202000000000, 0x4040404000000000, 0x8080808000000000, 0x101010000000000, 0x202020000000000, 0x404040000000000, 0x808080000000000, 0x1010100000000000, 0x2020200000000000, 0x4040400000000000, 0x8080800000000000, 0x101000000000000, 0x202000000000000, 0x404000000000000, 0x808000000000000, 0x1010000000000000, 0x2020000000000000, 0x4040000000000000, 0x8080000000000000, 0x100000000000000, 0x200000000000000, 0x400000000000000, 0x800000000000000, 0x1000000000000000, 0x2000000000000000, 0x4000000000000000, 0x8000000000000000, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		{0x8040201008040200, 0x80402010080400, 0x804020100800, 0x8040201000, 0x80402000, 0x804000, 0x8000, 0x0, 0x4020100804020000, 0x8040201008040000, 0x80402010080000, 0x804020100000, 0x8040200000, 0x80400000, 0x800000, 0x0, 0x2010080402000000, 0x4020100804000000, 0x8040201008000000, 0x80402010000000, 0x804020000000, 0x8040000000, 0x80000000, 0x0, 0x1008040200000000, 0x2010080400000000, 0x4020100800000000, 0x8040201000000000, 0x80402000000000, 0x804000000000, 0x8000000000, 0x0, 0x804020000000000, 0x1008040000000000, 0x2010080000000000, 0x4020100000000000, 0x8040200000000000, 0x80400000000000, 0x800000000000, 0x0, 0x402000000000000, 0x804000000000000, 0x1008000000000000, 0x2010000000000000, 0x4020000000000000, 0x8040000000000000, 0x80000000000000, 0x0, 0x200000000000000, 0x400000000000000, 0x800000000000000, 0x1000000000000000, 0x2000000000000000, 0x4000000000000000, 0x8000000000000000, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		{0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x0, 0xfe00, 0xfc00, 0xf800, 0xf000, 0xe000, 0xc000, 0x8000, 0x0, 0xfe0000, 0xfc0000, 0xf80000, 0xf00000, 0xe00000, 0xc00000, 0x800000, 0x0, 0xfe000000, 0xfc000000, 0xf8000000, 0xf0000000, 0xe0000000, 0xc0000000, 0x80000000, 0x0, 0xfe00000000, 0xfc00000000, 0xf800000000, 0xf000000000, 0xe000000000, 0xc000000000, 0x8000000000, 0x0, 0xfe0000000000, 0xfc0000000000, 0xf80000000000, 0xf00000000000, 0xe00000000000, 0xc00000000000, 0x800000000000, 0x0, 0xfe000000000000, 0xfc000000000000, 0xf8000000000000, 0xf0000000000000, 0xe0000000000000, 0xc0000000000000, 0x80000000000000, 0x0, 0xfe00000000000000, 0xfc00000000000000, 0xf800000000000000, 0xf000000000000000, 0xe000000000000000, 0xc000000000000000, 0x8000000000000000, 0x0},
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x0, 0x204, 0x408, 0x810, 0x1020, 0x2040, 0x4080, 0x8000, 0x0, 0x20408, 0x40810, 0x81020, 0x102040, 0x204080, 0x408000, 0x800000, 0x0, 0x2040810, 0x4081020, 0x8102040, 0x10204080, 0x20408000, 0x40800000, 0x80000000, 0x0, 0x204081020, 0x408102040, 0x810204080, 0x1020408000, 0x2040800000, 0x4080000000, 0x8000000000, 0x0, 0x20408102040, 0x40810204080, 0x81020408000, 0x102040800000, 0x204080000000, 0x408000000000, 0x800000000000, 0x0, 0x2040810204080, 0x4081020408000, 0x8102040800000, 0x10204080000000, 0x20408000000000, 0x40800000000000, 0x80000000000000, 0x0},
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x101, 0x202, 0x404, 0x808, 0x1010, 0x2020, 0x4040, 0x8080, 0x10101, 0x20202, 0x40404, 0x80808, 0x101010, 0x202020, 0x404040, 0x808080, 0x1010101, 0x2020202, 0x4040404, 0x8080808, 0x10101010, 0x20202020, 0x40404040, 0x80808080, 0x101010101, 0x202020202, 0x404040404, 0x808080808, 0x1010101010, 0x2020202020, 0x4040404040, 0x8080808080, 0x10101010101, 0x20202020202, 0x40404040404, 0x80808080808, 0x101010101010, 0x202020202020, 0x404040404040, 0x808080808080, 0x1010101010101, 0x2020202020202, 0x4040404040404, 0x8080808080808, 0x10101010101010, 0x20202020202020, 0x40404040404040, 0x80808080808080},
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x0, 0x100, 0x201, 0x402, 0x804, 0x1008, 0x2010, 0x4020, 0x0, 0x10000, 0x20100, 0x40201, 0x80402, 0x100804, 0x201008, 0x402010, 0x0, 0x1000000, 0x2010000, 0x4020100, 0x8040201, 0x10080402, 0x20100804, 0x40201008, 0x0, 0x100000000, 0x201000000, 0x402010000, 0x804020100, 0x1008040201, 0x2010080402, 0x4020100804, 0x0, 0x10000000000, 0x20100000000, 0x40201000000, 0x80402010000, 0x100804020100, 0x201008040201, 0x402010080402, 0x0, 0x1000000000000, 0x2010000000000, 0x4020100000000, 0x8040201000000, 0x10080402010000, 0x20100804020100, 0x40201008040201},
		{0x0, 0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0x0, 0x100, 0x300, 0x700, 0xf00, 0x1f00, 0x3f00, 0x7f00, 0x0, 0x10000, 0x30000, 0x70000, 0xf0000, 0x1f0000, 0x3f0000, 0x7f0000, 0x0, 0x1000000, 0x3000000, 0x7000000, 0xf000000, 0x1f000000, 0x3f000000, 0x7f000000, 0x0, 0x100000000, 0x300000000, 0x700000000, 0xf00000000, 0x1f00000000, 0x3f00000000, 0x7f00000000, 0x0, 0x10000000000, 0x30000000000, 0x70000000000, 0xf0000000000, 0x1f0000000000, 0x3f0000000000, 0x7f0000000000, 0x0, 0x1000000000000, 0x3000000000000, 0x7000000000000, 0xf000000000000, 0x1f000000000000, 0x3f000000000000, 0x7f000000000000, 0x0, 0x100000000000000, 0x300000000000000, 0x700000000000000, 0xf00000000000000, 0x1f00000000000000, 0x3f00000000000000, 0x7f00000000000000},
		{0x0, 0x100, 0x10200, 0x1020400, 0x102040800, 0x10204081000, 0x1020408102000, 0x102040810204000, 0x0, 0x10000, 0x1020000, 0x102040000, 0x10204080000, 0x1020408100000, 0x102040810200000, 0x204081020400000, 0x0, 0x1000000, 0x102000000, 0x10204000000, 0x1020408000000, 0x102040810000000, 0x204081020000000, 0x408102040000000, 0x0, 0x100000000, 0x10200000000, 0x1020400000000, 0x102040800000000, 0x204081000000000, 0x408102000000000, 0x810204000000000, 0x0, 0x10000000000, 0x1020000000000, 0x102040000000000, 0x204080000000000, 0x408100000000000, 0x810200000000000, 0x1020400000000000, 0x0, 0x1000000000000, 0x102000000000000, 0x204000000000000, 0x408000000000000, 0x810000000000000, 0x1020000000000000, 0x2040000000000000, 0x0, 0x100000000000000, 0x200000000000000, 0x400000000000000, 0x800000000000000, 0x1000000000000000, 0x2000000000000000, 0x4000000000000000, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}
	};

	int blocker;

	// Calculates all possible moves for the direction
	uint64_t moves = rayLookup[dir][index];
	// Calculates all the bits that are being blocked
	uint64_t blockers = moves & occupied;

	if (blockers) {
		// Finds the bit thats blocking the rest
		blocker = bitScan(blockers);
		// Removes all the blocked bits and removes the blocker if its of the same color
		moves ^= rayLookup[dir][blocker] | (pow2[blocker] & myPieces);
	}

	return moves;
}