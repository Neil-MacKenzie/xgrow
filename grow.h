

#define DEBUG 1
#define dprintf if (DEBUG) printf
#define d2printf if (DEBUG==2) printf

double drand48(); long lrand48(); 
double exp(); double log();

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

/* make off-by-one error less likely : include a boundary of empty */
/* cells that will never be modified                               */
#define Cell(i,j) cell[(i)+1][(j)+1]
/* note i,j here are indexed 0 <= i,j < (1<<P)                     */
/* 
   for periodic boundary conditions,  
   cell[0][*] cell[size+1][*] cell[*][0] cell[*][size+1]
   are maintained with the contents of the opposite wall;
   for non-periodic conditions, they are maintained as 0=empty
*/



/* macro definition of summed sticky end bond energy                    */
/* computes energy IF Cell(i,j) were n, given its current neighbors     */
/* assumes "fp" arg is a simple variable, but others can be expressions */
/* note that n != 0  and assumes 0 <= i,j < (1<<fp->P)                  */
#define Gse(fp,i,j,n) (                                \
        fp->tube->Gse_EW[ n ] [ fp->Cell(i,(j)-1) ] +  \
        fp->tube->Gse_EW[ fp->Cell(i,(j)+1) ] [ n ] +  \
        fp->tube->Gse_NS[ n ] [ fp->Cell((i)+1,j) ] +  \
        fp->tube->Gse_NS[ fp->Cell((i)-1,j) ] [ n ] )

/* similar definition to count the number of sides that are mismatched   */
/* also,  n != 0   and assumes 0 <= i,j < (1<<fp->P).                    */
/* this gives the number of mismatched bonds (not null bonds, not equal) */
/* NOT the number of tiles with mismatches.                              */
/* note that this counts twice: sum_i,j Mism(i,j) == 2ce # mism. bonds.  */
/* however, *if* the sum is accumulated during assembly, exactly when    */
/* the tile at i,j is being added, then it counts each mismatch ONCE.    */

#define Mism(fp,i,j,n) (                                                   \
 ((fp->tube->units)[n][1] != (fp->tube->units)[fp->Cell(i,(j)+1)][3] &&    \
  (fp->tube->units)[n][1]*(fp->tube->units)[fp->Cell(i,(j)+1)][3] > 0) +   \
 ((fp->tube->units)[n][3] != (fp->tube->units)[fp->Cell(i,(j)-1)][1] &&    \
  (fp->tube->units)[n][3]*(fp->tube->units)[fp->Cell(i,(j)-1)][1] > 0) +   \
 ((fp->tube->units)[n][2] != (fp->tube->units)[fp->Cell((i)+1,j)][0] &&    \
  (fp->tube->units)[n][2]*(fp->tube->units)[fp->Cell((i)+1,j)][0] > 0) +   \
 ((fp->tube->units)[n][0] != (fp->tube->units)[fp->Cell((i)-1,j)][2] &&    \
  (fp->tube->units)[n][0]*(fp->tube->units)[fp->Cell((i)-1,j)][2] > 0) )



typedef struct flake_struct {
  struct tube_struct *tube; /* contains tile set, reaction conditions,     */
                       /* time, event stats, scratch space...              */
                       /* all flakes are the same size, 2^(tube->P)        */
  unsigned char N, P;  /* # non-empty tile types; 2^P active cell grid     */

  unsigned char **cell;/* tile type at [i][j]; array of arrays             */
                       /* note 0 <= i,j <= 2^P+1, allowing for borders     */
  double ***rate;      /* hierarchical rates for events in non-empty cells */
                       /* rate[p][i][j] has 0 <= i,j <= 2^p                */
                       /* rate[P][i][j] = sum rates for Cell(i,j)          */
                       /* rate[p][i][j] =   sum rate[p+1][2*i+di][2*j+dj]  */
                       /*                 (di,dj in {0,1})                 */
                       /* rate[0][0][0] + k * sum conc[] = net event rate  */
  int ***empty;        /* hierarchical tally of number of empty cells      */
                       /* adjacent to some non-empty cell.                 */
                       /* for irreversible model, counts only if there is  */
                       /* a tile type that could stick here.               */  
  double flake_conc;   /* for depleting concentrations of monomers as they */
                       /* are incorporated; if 0, then no depletion occurs */
  double G;            /* cumulative energy of tile flake                  */
  int seed_i,seed_j;   /* special site which won't change                  */
  unsigned char seed_n; 
  long int events;     /* total on, off, hydrolysis events in this flake   */
  int tiles;           /* total number of tiles in this flake              */
  int mismatches;      /* number of se edges that don't agree              */
  struct flake_struct *next_flake;  /* for NULL-terminated linked list     */
  struct flake_tree_struct *tree_node;  /* for tree of flakes              */
  int flake_ID;        /* which flake is this (for display use only)       */
} flake;          

typedef struct flake_tree_struct {
  struct flake_tree_struct *left, *right, *up;    /* up==NULL iff root     */
  flake *fp;                         /* if fp != NULL, left==right==NULL   */
  int empty; double rate;            /* analogous to empty & rate in flake */
} flake_tree;


typedef struct tube_struct {
  int **units;         /* tile types */
  double *strength;    /* bond strengths */
  int num_bindings;    /* length of strength */
  
  unsigned char N, P;  /* # non-empty tile types; 2^P active cell grid     */

  int num_flakes;      /* how many flakes do we have here?                 */
  flake *flake_list;   /* for NULL-terminated linked list                  */
  flake_tree *flake_tree; /* binary tree for fast event selection          */

  unsigned char hydro; /* does this tile set use hydrolysis rules?         */
                       /* in this case, N must be even, and tiles          */
                       /* 1...N/2 are non-hydrolized; tiles N/2+1...N are  */
                       /* hydrolized, with weaker sticky end strengths     */
  double T;            /* threshold T for irreversible Tile Assembly Model */
                       /* used to prevent dissociation only if T>0         */
                       /* also, prevents incorrect association             */
                       /* on-rates & off-rates are calculated as usual,    */
                       /* but events violating the model are discarded     */
  double k;            /* forward rate constant for on-events.             */
  double kas,kao,      /* f.r.c (ratio to k) for "hydrolysis" spontaneous, */
         kam,kae,kah;  /* and when input se are mismatched, empty, or      */
                       /* input tiles are "hydrolyized". kao is the ratio  */
                       /* of output-triggered/input-triggered rates.       */
  double *conc;        /* concentration of each tile type n = 1...N        */
                       /* conc[0] = sum conc[n] from n = 1...N             */
                       /* conc[n] =def= exp(-Gmc[n])                       */
  double *Gcb;         /* neg std free energy of chemical bonds in tile    */
                       /* (zero typically; pos for hydrolysis models)      */
  double **Gse_EW;     /* bond energy between east-west oriented tiles     */
                       /* Gse_EW[n1][n2] =  n1's west se  : n2's east se   */
  double **Gse_NS;     /* Gse_NS[n1][n2] =  n1's south se : n2's north se  */
                       /* NOTE n could be empty tile, for which Gse = 0    */
                       /* coordinate system is: i+,j+ moves S,E            */
  double t;            /* cumulative time in seconds                       */
  long int events;     /* cumulative number of events                      */
  long int stat_a,stat_d,/* tally of number of association, dissociation,  */
      stat_h,stat_f;   /* "hydrolysis", and "fission" events               */
  double *rv;          /* scratch space, size fp->N+1                      */
  int *Fnext, *Fgroup; /* size x size array for fill scratch space         */
} tube;          

extern int periodic;    /* simulation on torus */
extern int wander;      /* of seed tile designation */
extern int fission_allowed; /* allow dissociation that breaks flake in two? */

tube *init_tube(unsigned char P, unsigned char N, int num_bindings);
flake *init_flake(unsigned char P, unsigned char N,
 int seed_i, int seed_j, int seed_n, double Gfc);
flake *free_flake(flake *fp);
void free_tube(tube *tp);
void insert_flake(flake *fp, tube *tp);
void print_tree(flake_tree *ftp, int L, char s);
void clean_flake(flake *fp, double X, int iters);
void set_params(tube *tp, int** units, double* strength, double* stoic,
 int hydro, double k, double Gmc, double Gse,
 double Gmch, double Gseh, double Ghyd, 
 double Gas, double Gam, double Gae, double Gah, double Gao, double T);
void reset_params(tube *tp, double old_Gmc, double old_Gse,
 double new_Gmc, double new_Gse);
void recalc_G(flake *fp);
double calc_dG_bonds(flake *fp);
int calc_perimeter(flake *fp);
void update_rates(flake *fp, int ii, int jj);
void update_tube_rates(flake *fp);
void change_cell(flake *fp, int i, int j, unsigned char n);
void simulate(tube *tp, int events, double tmax, int emax, int smax);
void linear_simulate( double ratek, double Gmc, double Gse,
                      double tmax, int emax, int smax);





