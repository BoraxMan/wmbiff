
#define SUBJ_LEN 50
#define FROM_LEN 22
struct msglst {
  struct msglst *next;
  char subj[SUBJ_LEN];
  char from[FROM_LEN];
};

void msglst_show(Pop3 pc, int x, int y);
void msglst_hide(void);
void msglst_redraw(void);
