#ifndef SRC_EVAL_H_
#define SRC_EVAL_H_


#define MAX_SCORE 10000
#define PHASES 2

enum {OPENING, ENDGAME};

typedef struct {
	int nPawns;
	int nKnights;
	int nBishops;
	int nRooks;
	int nQueens;
} Count;


int finalEval(Board board, const int depth);
int eval(Board board);


#endif /* SRC_EVAL_H_ */