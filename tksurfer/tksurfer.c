/*============================================================================
  Copyright (c) 1996 Martin Sereno and Anders Dale
=============================================================================*/
#define TCL
#define TKSURFER 
#define TCL8
#include "xDebug.h"
#include "proto.h"
#include "mrisurf.h"
#include "const.h"
#include "fio.h"
#include "error.h"
#include "tritri.h"
#include "diag.h"
#include "rgb_image.h"
#include "const.h"

static void set_value_label_name(char *label_name, int field) ;
static void drawcb(void) ;
static void read_imag_vals(char *fname) ;
static void read_soltimecourse(char *fname) ;
static void sol_plot(int timept, int plot_type) ;

static void remove_triangle_links(void) ;
static int draw_curvature_line(void) ;

static void move_window(int x,int y) ;
int MRIStransformBrain(MRI_SURFACE *mris, 
           float exx, float exy, float exz, 
           float eyx, float eyy, float eyz, 
           float ezx, float ezy, float ezz) ;
static void save_rgbfile(char *fname, int width, int height, 
                         unsigned short *red, 
                         unsigned short *green, unsigned short *blue) ;
static void grabPixels(unsigned int width, unsigned int height,
                       unsigned short *red, unsigned short *green, 
                       unsigned short *blue) ;

static int  is_val_file(char *fname) ;
void show_flat_regions(char *surf_name, double thresh) ;
void val_to_mark(void) ;
void curv_to_val(void) ;
int read_curv_to_val(char *fname) ;
int read_parcellation(char *parc_fname, char *lut_fname) ;
int read_and_smooth_parcellation(char *parc_fname, char *lut_fname,
                                 int siter, int miter) ;
void read_stds(int cond_no) ;
void val_to_curv(void) ;
void val_to_stat(void) ;
void stat_to_val(void) ;

static void label_to_stat(void) ;
static void f_to_t(void) ;
static void t_to_p(int dof) ;
static void f_to_p(int numer_dof, int denom_dof) ;
int mask_label(char *label_name) ;

MRI_SURFACE *mris = NULL, *mris2 = NULL ;
static char *sdir = NULL ;
static char *sphere_reg ;

#define QUAD_FILE_MAGIC_NUMBER      (-1 & 0x00ffffff)
#define TRIANGLE_FILE_MAGIC_NUMBER  (-2 & 0x00ffffff)
#define NEW_VERSION_MAGIC_NUMBER  16777215

/*#include "surfer.c"*/


/* start of surfer.c */


#if defined(TCL) && defined(TKSURFER)
/*============================================================================
  Copyright (c) 1996 Anders Dale and Martin Sereno
  =============================================================================*/
#include <tcl.h>
#include <tk.h>
#include <tix.h>
/* begin rkt */
#include <blt.h>
#include <signal.h>
/* end rkt */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <GL/gl.h>
#include "typedefs.h"
#include "mgh_matrix.h"
#include "label.h"
/*#include "surfer.h"*/
#include "fio.h"
#include "MRIio.h"
/*#include "volume_io.h"*/
#include "rgb_image.h"
#include "transform.h"
#include "proto.h"
#include "diag.h"
#include "macros.h"
#include "utils.h"
#include "machine.h"

/* used to ifdef Linux'd, but should always be true */
#ifndef OPENGL
#define OPENGL
#endif
#define TCL

char *Progname = "surfer" ;

#if defined(Linux) || defined(SunOS) || defined(sun)
#define GL_ABGR_EXT                         0x8000

#endif

#ifdef TCL
#  define PR   {if(promptflag){fputs("% ", stdout);} fflush(stdout);}
#else
#  define PR
#endif

#ifdef OPENGL
#include <X11/keysym.h>
#include "xwindow.h"
#  define RGBcolor(R,G,B)  glColor3ub((GLubyte)(R),(GLubyte)(G),(GLubyte)(B))
#  define translate(X,Y,Z) glTranslatef(X,Y,Z)
/*one list OK;GL_POLYGON slower*/
#if VERTICES_PER_FACE == 4
#  define bgnpolygon()     glBegin(GL_QUADS)
#else
#  define bgnpolygon()     glBegin(GL_TRIANGLES)
#  define bgnquadrangle()     glBegin(GL_QUADS)
#endif
#  define bgnline()        glBegin(GL_LINES)
#  define bgnpoint()       glBegin(GL_POINTS)
#  define v3f(X)           glVertex3fv(X)
#  define n3f(X)           glNormal3fv(X)
#  define endpolygon()     glEnd()
#  define endline()        glEnd()
#  define endpoint()       glEnd()
#  define swapbuffers()    tkoSwapBuffers()
#  define lrectread(X0,Y0,X1,Y1,P) \
            glReadPixels(X0,Y0,X1-X0+1,Y1-Y0+1,GL_ABGR_EXT,GL_UNSIGNED_BYTE,P)
#  define czclear(A,B)     glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
#  define pushmatrix()     glPushMatrix()
#  define popmatrix()      glPopMatrix()
#  define linewidth(X)     glLineWidth((float)(X))
#  define getmatrix(X)     glGetFloatv(GL_MODELVIEW_MATRIX,(float *)(X))
#  define loadmatrix(X)    glLoadMatrixf((float *)(X))
#  define getorigin(X,Y)   *(X) = w.x; *(Y) = 1024 - w.y - w.h /*WINDOW_REC w;*/
#  define getsize(X,Y)     *(X) = w.w; *(Y) = w.h
#else
#  include <gl.h>
#  include <device.h>
#endif

/* begin rkt */
/* make these decls if the headers are screwed up */
#ifndef Blt_Init
int Blt_Init ( Tcl_Interp* interp );
#endif 
#ifndef Blt_SafeInit
int Blt_SafeInit ( Tcl_Interp* interp );
#endif 

#ifndef Tix_SafeInit
int Tix_SafeInit ( Tcl_Interp* interp );
#endif 
/* end rkt */

#ifndef TCL
#  ifndef OPENGL
#    include <gl.h>     /* non-TCL event loop not moved=>X for OpenGL */
#    include <device.h> /* ditto */
#  endif
#endif

#ifndef SQR
#define SQR(x)       ((x)*(x))
#endif

#define MATCH(A,B)   (!strcmp(A,B))
#define MATCH_STR(S) (!strcmp(str,S))
#ifndef FALSE
#  define FALSE  0
#endif
#ifndef TRUE
#  define TRUE   1
#endif

#define IMGSIZE      256
#define NUMVALS      256
#define MAXIM        256
#define MAXMARKED    200000
#define NLABELS      256
#define CMFBINS       30
#define GRAYBINS      15 
#define GRAYINCR     0.25   /* mm */
#define MAXVAL       60.0
#define MOTIF_XFUDGE   8
#define MOTIF_YFUDGE  32
#define NAME_LENGTH   STRLEN
#define MAX_DIR_DEPTH  30
#define SCALE_UP_MOUSE   2.0
#define SCALE_UP_LG      1.25
#define SCALE_UP_SM      1.05
#define BLINK_DELAY 30
#define BLINK_TIME 20

#define MESH_LINE_PIX_WIDTH    1
#define CURSOR_LINE_PIX_WIDTH  2
#define SCALEBAR_WIDTH        6 
#define SCALEBAR_BRIGHT     128 
#define SCALEBAR_MM          10 
#define SCALEBAR_XPOS       0.9
#define SCALEBAR_YPOS       0.9
#define COLSCALEBAR_XPOS    0.95
#define COLSCALEBAR_YPOS   -0.70
#define COLSCALEBAR_WIDTH   0.05
#define COLSCALEBAR_HEIGHT  0.5

#define TMP_DIR         "tmp"              /* relative to subjectsdir/pname */
#define LABEL_DIR       "label"            /* ditto */
#define IMAGE_DIR       "rgb"              /* ditto */
#define BRAINSURF_DIR   "surf"             /* ditto */
#define BEM_DIR         "bem"              /* ditto */
#define DEVOLT1_DIR     "mri/T1"           /* ditto */
#define FILLDIR_STEM    "morph/fill"       /* ditto */
#define CURVDIR_STEM    "morph/curv"       /* ditto */
#define EEGMEG_DIR      "eegmeg"           /* ditto */
#define TRANSFORM_DIR   "mri/transforms"   /* ditto */
#define FS_DIR          "fs"               /* relative to session dir */
#define TALAIRACH_FNAME "talairach.xfm"    /* relative to TRANSFORM_DIR */

/* surfcolor */
#define NONE                 0
#define CURVATURE_OR_SULCUS  1
#define AREAL_DISTORTION     2
#define SHEAR_DISTORTION     3
/* colscales for: set_{complexval_,"",stat_,positive_,signed_}color */
#define COLOR_WHEEL         0   /* complexval */
#define HEAT_SCALE          1   /* stat,positive,"" */
#define CYAN_TO_RED         2   /* stat,positive,"" */
#define BLU_GRE_RED         3   /* stat,positive,"" */
#define TWOCOND_GREEN_RED   4   /* complexval */
#define JUST_GRAY           5   /* stat,positive,"" */
#define BLUE_TO_RED_SIGNED  6   /* signed */
#define GREEN_TO_RED_SIGNED 7   /* signed */
#define RYGB_WHEEL          8   /* complexval */
#define NOT_HERE_SIGNED     9   /* signed */
/* set_color modes */
#define GREEN_RED_CURV     0   /* default: red,green,gray curvature */
#define REAL_VAL           1   /* specified further by some colscales */
#define FIELDSIGN_POS      4   /* blue */
#define FIELDSIGN_NEG      5   /* yellow */
#define BORDER             6   /* yellow */
#define MARKED             7   /* white */


/* project directions in MRI coords (flatten) */
#define RIGHT       1    /* x */
#define LEFT       -1 
#define ANTERIOR    2    /* y */
#define POSTERIOR  -2
#define SUPERIOR    3    /* z */
#define INFERIOR   -3 
/* draw_spokes options */
#define RADIUS    0
#define THETA     1
/* for curv/fill image flags  */
#define CURVIM_FILLED        0x01
#define CURVIM_DEFINED       0x02
#define CURVIM_DEFINED_OLD   0x04
#define CURVIM_FIXEDVAL      0x08
/* truncated max min for normalize curv (scales byte image) */
#define CURVIM_NORM_MIN  -2.0
#define CURVIM_NORM_MAX   2.0
/* filltypes */
#define RIPFILL     0 
#define STATFILL    1 
#define CURVFILL    2

/* vrml modes */
#define VRML_POINTS             0 
#define VRML_QUADS_GRAY         1
#define VRML_QUADS_GRAY_SMOOTH  2
#define VRML_QUADS_CURV         3
#define VRML_QUADS_CURV_SMOOTH  4

/* begin rkt */
/* value field labels for swap_vertex_fields(), also used in tcl script */
#define FIELD_CURV      0
#define FIELD_CURVBAK   1
#define FIELD_VAL       2
#define FIELD_IMAG_VAL  3
#define FIELD_VAL2      4
#define FIELD_VALBAK    5
#define FIELD_VAL2BAK   6
#define FIELD_STAT      7

/* label sets for update_labels, also used in tcl script */
#define LABELSET_CURSOR     0
#define LABELSET_MOUSEOVER  1

/* label fields for update_labels, also used in tcl script */
enum { 
  LABEL_VERTEXINDEX = 0,
  LABEL_DISTANCE,
  LABEL_COORDS_RAS,
  LABEL_COORDS_MNITAL,
  LABEL_COORDS_TAL,
  LABEL_COORDS_INDEX,
  LABEL_COORDS_NORMAL,
  LABEL_COORDS_SPHERE_XYZ,
  LABEL_COORDS_SPHERE_RT,
  LABEL_CURVATURE,
  LABEL_FIELDSIGN,
  LABEL_VAL,
  LABEL_VAL2,
  LABEL_VALBAK,
  LABEL_VAL2BAK,
  LABEL_VALSTAT,
  LABEL_IMAGVAL,
  LABEL_MEAN,
  LABEL_MEANIMAG,
  LABEL_STDERROR,
  LABEL_AMPLITUDE,
  LABEL_ANGLE,
  LABEL_DEGREE,
  LABEL_LABEL,
  LABEL_ANNOTATION,
  LABEL_MRIVALUE,
  LABEL_PARCELLATION_NAME
};

/* extra error codes */
#define ERROR_UNDO_ACTION -998      /* invalid action struct */
#define ERROR_UNDO_LIST_STATE  -999 /* inapproriate operation for undo
               list state */
#define ERROR_ARRAY       -1000     /* problem with xGrowableArray */
#define ERROR_FUNC        -1001     /* other error with func volume */
#define ERROR_NOT_INITED  -1002     /* something that should have been
               previously inited, etc, hasn't been */
#define ERROR_CLUT        -1003     /* error with mriColorLookupTable */

/* end rkt */

int file_type = MRIS_BINARY_QUADRANGLE_FILE ;

#if 0
FLOATTYPE **E,**B,**D,**We,**Wb,**Wd;

FLOATTYPE **A,**At;
FLOATTYPE **C;
FLOATTYPE *R;
FLOATTYPE **M,**Mi,**Mb;
FLOATTYPE **W;
FLOATTYPE **_Y,**Yi;
FLOATTYPE **Cov;
FLOATTYPE *dipval,*dipval2,*eegval,*eegval2;
FLOATTYPE *megval,*megval2,*depval,*depval2;
FLOATTYPE *dipvalbak,*eegvalbak,*megvalbak,*depvalbak;
FLOATTYPE *eegx,*eegy,*eegz,*megx,*megy,*megz,*depx,*depy,*depz;
FLOATTYPE *tmp1,*tmp2;

int *dipindex;

int ndip,neeg,nmeg,ndep,nsens;

float dipscale,eegscale,megscale,depscale,sensscale;
float dfthresh,dfsquash;
#endif

/* global names for tcl links */
char val_dir[STRLEN] ; /* directory that value (.w) file was read from */
char *subjectsdir; /* $subjectsdir: from getenv */
char *srname;      /* $session(dir!) abs:#/951206MS/image,#/MARTY0928/08192*/
char *pname;       /* name: $home = $subjectsdir/$name */
char *stem;        /* hemisphere (head: e.g., rh from rh.wmsooth) */
char *ext;         /* surf (suffix: e.g., wmsmooth from rh.wmsmooth) */
char *fpref;       /* $home/surf/hemi. (for assembly) */
char *ifname;      /* $home/surf/hemi.surf: read */
char *if2name;     /* $home/surf/hemi.surf: read (curv target) */
char *ofname;      /* set: $home/surf/hemi.surf: write */
char *cfname;      /* $home/surf/hemi.curv */
char *cf2name;     /* $home/surf/hemi.curv--target surface */
char *sfname;      /* $home/surf/hemi.sub */
char *dfname;      /* $home/surf/hemi.dip */
char *kfname;      /* $home/surf/hemi.sulc */
char *mfname;      /* $home/mri/T1/COR- */
char *vfname;      /* set: $home/surf/hemi.val */
char *fsfname;     /* $session/fs/hemi.fs */
char *fmfname;     /* $session/fs/hemi.fm */
char *pname2;      /* name2 */
char *stem2;       /* hemi2. */
char *ext2;        /* surf2 */
char *tf2name;     /* $home/morph/{fill}hemi2.surf2/COR- */
char *afname;      /* $home/surf/hemi.area */
char *pfname;      /* $home/surf/hemi.patch */
char *tfname;      /* (dir!) ~/name/tmp/ */
char *gfname;      /* (dir!) ~/name/rgb/ */
char *sgfname;     /* (dir!) set: get from cwd: $session/rgb/ */
char *agfname;     /* $home/rgb/tksurfer.rgb */
char *fifname;     /* prefix: $home/morph/{fill}.hemi.surf/COR- */
char *cif2name;    /* prefix: $targhome/morph/{curv}.hemi.surf/COR- */
char *rfname;      /* set: scriptfile (undef at start!) */
char *nfname;      /* annotation.rgb */
char *orfname;     /* $home/surf/hemi.orig */
char *paint_fname; /* $home/surf/hemi.smoothwm */
char *elfname;     /* $home/surf/hemi.1000a2ell */
char *lfname;      /* $home/label/hemi-name.label */
char *vrfname;     /* $home/name/surf/rh.smoothwm.wrl */
char *xffname;     /* $home/name/mri/transforms/TALAIRACH_FNAME */
char *orig_suffix = "orig" ;

FILE *fpvalfile;              /* mult frames */
int openvalfileflag = FALSE;
int twocond_flag = FALSE ;    /* are we in two-condition mode */
int disc_flag = FALSE ;
int cond0 = 0 ;
int cond1 = 1 ;
int openedcmpfilenum = 0;
int cmpfilenamedframe = -1;
float cmpfilenamedfirstlat;
FILE *fpcmpfilenamed;        /* for open cmp movie file */

unsigned long *framebuff, *framebuff2, *framebuff3;
unsigned char *binbuff;
int framenum = 0;
long frame_xdim = 600;
long frame_ydim = 600;
int ilat = 0;  /* latency */

int sub_num ;
#if 0
int vertex_index, face_index;
FACE *face;
VERTEX *vertex; int vertex2_index,face2_index;
face2_type *face2;
VERTEX *vertex2;
float total_area;
#endif



int xnum=256,ynum=256;
unsigned long bufsize;
unsigned char **im[MAXIM];
unsigned char **fill[MAXIM];
unsigned char *buf = NULL;  /* scratch */
int imnr0,imnr1,numimg;
int wx0=650,wy0=416;

unsigned char **curvim[MAXIM];
unsigned char **ocurvim[MAXIM];
unsigned char **curvimflags[MAXIM];
#if 0
float curvmin2,curvmax2;
float curvmin,curvmax;
#endif
float avgcurvmin,avgcurvmax;
double icstrength=1.0;   /* icurv force */
int curvim_allocated=FALSE;
int curvim_averaged=0;

float sf=0.55;      /* initial scale factor */
float zf=1.0;       /* current scale */

double scalebar_xpos = SCALEBAR_XPOS;
double scalebar_ypos = SCALEBAR_YPOS;
int scalebar_bright = SCALEBAR_BRIGHT;
double colscalebar_width = COLSCALEBAR_WIDTH;
double colscalebar_height = COLSCALEBAR_HEIGHT;
double colscalebar_xpos = COLSCALEBAR_XPOS;
double colscalebar_ypos = COLSCALEBAR_YPOS;

double cthk = 1.0;  /* cortical thickness (mm) */
float ostilt = 1.0; /* outside stilt length (mm) */
int mingm = 50;     /* outer edge gray matter */

double mstrength = 0.125;
double mmid = 45.0;       /* was fzero=35.0 */
double mslope = 0.05;     /* was fsteepness */
double whitemid = 45.0;   /* was whitezero=35.0 */
double graymid = 30.0;    /* was grayzero=25.0; */
double fslope = 0.50;     /* was fsquash */
double fmid = 4.0;        /* was fzero */
double foffset = 0 ;
double fcurv = 0.00;
double cslope = 1.00;     /* was fsquash */
double cmid = 0.0;
double cmax = 100000.0f ;
double cmin = -100000.0f ;

float cup = 0.1;   /* dist cursor above surface */
float mup = 0.05;  /* dist edge above surface */
float pup = 0.07;  /* dist point above surface */

int mesh_linewidth = MESH_LINE_PIX_WIDTH ;
int meshr = 255;   /* mesh color */
int meshg = 255;
int meshb = 255;

float xmin,xmax;
float ymin,ymax;
float zmin,zmax;
float st,ps,fov,xx0,xx1,yy0,yy1,zz0,zz1;
float dipscale = 1.0;

static int selection = -1;
int nmarked = 0;
int marked[MAXMARKED];

LABEL *area = NULL ;

int autoflag = FALSE;
int autoscaleflag = FALSE;
int MRIflag = TRUE;
int MRIloaded = TRUE;
int electrodeflag = FALSE;
int explodeflag = FALSE;
int expandflag = FALSE;
int momentumflag = FALSE;
int sulcflag = FALSE;
int avgflag = FALSE;
int stiffnessflag = FALSE;
int areaflag = FALSE;
int flag2d = FALSE;
int shrinkfillflag = FALSE;
int ncthreshflag = FALSE;
int complexvalflag = FALSE;
int fieldsignflag = FALSE;
int wholeflag = FALSE;
int overlayflag = FALSE;
int verticesflag = FALSE;
int revfsflag = FALSE;
int revphaseflag = FALSE;
int invphaseflag = FALSE;
int rectphaseflag = FALSE;
int truncphaseflag = FALSE;
int scalebarflag = FALSE;
int colscalebarflag = FALSE;
int surfaceflag = TRUE;
int pointsflag = FALSE;
int statflag = FALSE; /* vertex (fMRI) stats read in ? */
int isocontourflag = FALSE;
int phasecontourflag = FALSE;
int curvimflag = FALSE;
int curvimloaded = FALSE;
int curvloaded = FALSE;
int secondsurfaceloaded = FALSE;
int origsurfloaded = FALSE ;
int annotationloaded = FALSE ;
int canonsurfloaded = FALSE ;
int canonsurffailed = FALSE ;
int sphericalsurfloaded = FALSE ;
int white_surf_loaded = FALSE ;
int inflated_surf_loaded = FALSE ;
int pial_surf_loaded = FALSE ;
int secondcurvloaded = FALSE;
int doublebufferflag = FALSE;
int openglwindowflag = FALSE;
int blinkflag = FALSE;
int renderoffscreen = FALSE;
int renderoffscreen1 = FALSE;
int blackcursorflag = FALSE;
int bigcursorflag = FALSE;
int vrml2flag = FALSE;
int showorigcoordsflag = FALSE;
int scrsaveflag = FALSE;
int phasecontourmodflag = FALSE;
int promptflag = FALSE;
int followglwinflag = TRUE;
int initpositiondoneflag = FALSE;
/* begin rkt */
int curvflag = TRUE; /* draw curv if loaded */
int mouseoverflag = TRUE; /* show mouseover information */
int redrawlockflag = FALSE; /* redraw on window uncover events */
int simpledrawmodeflag = TRUE; /* draw based on scalar values */
Tcl_Interp *g_interp = NULL;

int curwindowleft = 0; /* keep track of window position, updated on move */
int curwindowbottom = 0;
int dontloadspherereg = FALSE; /* if true, don't try loading sphere.reg */
/* end rkt */

int blinkdelay = BLINK_DELAY;
int blinktime = BLINK_TIME;

float contour_spacing[3];
double phasecontour_min = 0.3;
double phasecontour_max = 0.35;
int phasecontour_bright = 255;

int drawmask = 1;  /* same as next 6 */
int surfcolor = CURVATURE_OR_SULCUS;
int shearvecflag = FALSE;
int normvecflag = FALSE;
int movevecflag = FALSE;
int project = NONE;  /* flatten */

int computed_shear_flag = FALSE;

double update = 0.9;
double decay = 0.9;
float cweight = 0.33;
float aweight = 0.33;
float dweight = 0.33;
float epsilon = 0.0001;

double dip_spacing = 1.0;

int nrip = 0;
double stressthresh = 1e10;
float maxstress = 1e10;
float avgstress = 0;
int senstype = 0;
double fthresh = 2.0;
double wt = 0.5;
double wa = 0.5;
double ws = 0.5;
double wn = 0.5;
double wc = 0.0;
double wsh = 0.5;
double wbn = 0.5;
double ncthresh = 0.02;

int fixed_border = FALSE;
float fillscale = 1.5;
float curvscale;
double angle_offset = 0.0;
double angle_cycles = 1.0;
int smooth_cycles = 5;
int colscale = HEAT_SCALE;   /* changed by BRF - why was it COLOR_WHEEL??? */
double blufact = 1.0;
double cvfact = 1.5;
double fadef = 0.7;


float normal1[3] = {1.0,0.0,0.0};
float normal2[3] = {0.0,1.0,0.0};
float xpos[3] = {1.0,0.0,0.0};
float xneg[3] = {-1.0,0.0,0.0};
float ypos[3] = {0.0,1.0,0.0};
float yneg[3] = {0.0,-1.0,0.0};
float zpos[3] = {0.0,0.0,1.0};
float zneg[3] = {0.0,0.0,-1.0};
float v1[3],v2[3],v3[3],v4[3];
float v1[3],v2[3],v3[3],v4[3];

float gradavg[NLABELS][CMFBINS],gradsum[NLABELS][CMFBINS];
float gradnum[NLABELS][CMFBINS],gradtot[NLABELS];
float val1avg[NLABELS][CMFBINS],val2avg[NLABELS][CMFBINS];
float angleavg[NLABELS][CMFBINS];
float valnum[NLABELS][CMFBINS];
float gradx_avg[NLABELS],grady_avg[NLABELS],valtot[NLABELS],radmin[NLABELS];

double dipavg,dipvar,logaratavg,logaratvar,logshearavg,logshearvar;
double dipavg2;

#if 0
float xlo,xhi,ylo,yhi,zlo,zhi,xctr,yctr,zctr;
float xctr2,yctr2,zctr2;
#endif

#define PLOT_XDIM 200
#define PLOT_YDIM 200
#define PLOT_XFUDGE 7
#define PLOT_YFUDGE 30
#define MAX_RECFILES 20
#define MAX_NPLOTLIST 100

int *sol_dipindex,sol_neeg=0,sol_nmeg=0,sol_nchan=0,sol_ndipfiles=0,sol_ndip;
FLOATTYPE **sol_W,**sol_A,**sol_M,**sol_Mi,**sol_Data[MAX_RECFILES],**sol_NoiseCovariance;
FLOATTYPE *sol_pval,*sol_prior,*sol_sensval,*sol_sensvec1,*sol_sensvec2;
FLOATTYPE *sol_lat,**sol_dipcmp_val[MAX_RECFILES],*sol_dipfact;
int sol_plotlist[MAX_NPLOTLIST],sol_ndec=0,sol_rectpval=0;
int sol_ntime,sol_ptime,sol_nnz,sol_nperdip,sol_plot_type=1,sol_proj_type=0;
int sol_allocated=FALSE,sol_nrec=0,sol_nplotlist=0,sol_trendflag=TRUE;
double sol_sample_period,sol_epoch_begin_lat,sol_snr_rms=10;
double sol_baseline_period=100,sol_baseline_end=0,sol_lat0=0,sol_lat1=100000;
double sol_loflim=2.0,sol_hiflim=10.0;
double sol_pthresh=0.0,sol_pslope=0.0,sol_maxrat=10.0;
int vertex_nplotlist=0,vertex_plotlist[MAX_NPLOTLIST];

#define LIGHT0_BR  0.4 /* was 0.2 */
#define LIGHT1_BR  0.0 
#define LIGHT2_BR  0.6 /* was 0.3 */
#define LIGHT3_BR  0.2 /* was 0.1 */
#define OFFSET 0.25   /* was 0.15 */
#define BACKGROUND 0x00000000
double offset = OFFSET;
double light0_br,light1_br,light2_br,light3_br;

float idmat[4][4] = {
  {1.0,0.0,0.0,0.0},  /* Matrix idmat = */
  {0.0,1.0,0.0,0.0},
  {0.0,0.0,1.0,0.0},
  {0.0,0.0,0.0,1.0}};

/* accumulate really_ tranforms here */
float reallymat[4][4] = {
  {1.0,0.0,0.0,0.0},   /* Matrix reallymat = */
  {0.0,1.0,0.0,0.0},
  {0.0,0.0,1.0,0.0},
  {0.0,0.0,0.0,1.0}};

/* Talairach stuff */
LINEAR_TRANSFORM_ARRAY  *lta ;
int               transform_loaded = 0 ;

/* parcellation stuff */
static char parc_red[256] ;
static char parc_green[256] ;
static char parc_blue[256] ;
static char *parc_names[256] ;
static int parc_flag = 0 ;

static double dlat = 0 ;
static int surface_compiled = -1 ;
static int use_display_lists = 0 ;
static int FS_Brain_List = 1;
static int vertex_array_dirty = 0;
#ifndef IRIX
static int use_vertex_arrays = 1;
#define USE_VERTEX_ARRAYS 1
#else
static int use_vertex_arrays = 0;
#undef USE_VERTEX_ARRAYS 
#endif

static int color_scale_changed = TRUE;

/*---------------------- PROTOTYPES (should be static) ----------------*/
#ifdef TCL
int do_one_gl_event(Tcl_Interp *interp) ;
#endif
void compute_CMF(void) ;
void fix_nonzero_vals(void) ;
void smooth_momentum(int niter) ;
void smooth_logarat(int niter) ;
void smooth_shear(int niter) ;
void smooth_boundary_normals(int niter) ;
void scaledist(float sf) ;
float rtanh(float x) ;
void shrink(int niter) ;
void curv_shrink_to_fill(int niter) ;
void shrink_to_fill(int niter) ;
void transform(float *xptr, float *yptr, float *zptr, float nx, float ny, 
               float nz, float d) ;
void really_translate_brain(float x, float y, float z) ;
void really_scale_brain(float x, float y, float z) ;
void really_rotate_brain(float a, char axis) ;
void align_sphere(MRI_SURFACE *mris) ;
void really_align_brain(void) ;  /* trans cent first -> cent targ */
void really_center_brain(void) ;
void really_center_second_brain(void) ;
void print_real_transform_matrix(void) ;
void write_really_matrix(char *dir) ;
void read_really_matrix(char *dir) ;
void flatten(char *dir) ;
void area_shrink(int niter) ; /* consider area */
void shrink2d(int niter) ;
void sphere_shrink(int niter, float rad) ;
void ellipsoid_project(float a, float b, float c) ;
void ellipsoid_morph(int niter, float a, float b, float c) ;
void ellipsoid_shrink(int niter, float a, float b, float c) ;
void ellipsoid_shrink_bug(int niter, float rad, float len) ;
void compute_curvature(void) ;
void clear_curvature(void) ;
void normalize_area(void) ;
void normalize_surface(void) ;
void load_brain_coords(float x, float y, float z, float v[]) ;
int outside(float x,float y, float z) ;
void draw_surface(void) ;
void draw_ellipsoid_latlong(float a, float b, float c) ;
void draw_second_surface(void) ;
void draw_scalebar(void) ;
void draw_colscalebar(void) ;
void set_stat_color(float f, float *rp, float *gp, float *bp, float tmpoffset);
void set_positive_color(float f, float *rp, float *gp, float *bp, 
                        float tmpoffset) ;
void set_signed_color(float f, float *rp, float *gp,float *bp,float tmpoffset);
void set_color(float val, float curv, int mode) ;
void set_complexval_color(float x, float y, float stat, float curv) ;
void draw_spokes(int option) ;
void set_vertex_color(float r, float th, int option) ;
void set_color_wheel(float a, float a_offset, float a_cycles, int mode, 
                     int logmode, float fscale) ;
void restore_ripflags(int mode) ;
void floodfill_marked_patch(int filltype) ;
void clear_ripflags(void) ;
void cut_marked_vertices(int closedcurveflag) ;
void cut_plane(void) ;
void cut_vertex(void) ;
void cut_line(int closedcurveflag) ;
void plot_curv(int closedcurveflag) ;
void draw_fundus(int bdry_index) ;
void plot_marked(char *fname) ;
void put_retinotopy_stats_in_vals(void) ;
void draw_vector(char *fname) ;
void clear_vertex_marks(void) ;
void clear_all_vertex_marks(void) ;
void mark_vertex(int vindex, int onoroff) ;
void mark_face(int fno) ;
void mark_annotation(int selection) ;
void mark_faces(int vno) ;
void prompt_for_parameters(void) ;
void prompt_for_drawmask(void) ;
void fill_triangle(float x0,float y0,float z0,float x1,float y1,float z1,
                   float x2,float y2,float z2) ;
void fill_surface(void) ;
void fill_second_surface(void) ;
void resize_buffers(long frame_xdim, long frame_ydim) ;
void open_window(char *name) ;
void swap_buffers(void); 
void to_single_buffer(void) ;
void to_double_buffer(void) ;
void do_lighting_model(float lite0,float lite1,float lite2,float lite3,
                       float newoffset) ;
void make_filenames(char *lsubjectsdir,char *lsrname,char *lpname,char *lstem,
                    char *lext) ;
void print_help_surfer(void) ;
void print_help_tksurfer(void) ;
int Surfer(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[]);
int main(int argc, char *argv[]) ;
double dipval(int cond_num, int nzdip_num, int ilat) ;
void read_ncov(char *fname) ;
void regularize_matrix(FLOATTYPE **m, int n, float fact) ;
void read_iop(char *fname, int dipfilenum) ;
void read_rec(char *fname) ;
void filter_recs(void) ;
void filter_rec(int np) ;
void remove_trend(FLOATTYPE *v, int n) ;
void compute_timecourses(void) ;
void compute_timecourse(int num) ;
void plot_all_time_courses(void) ;
void read_plot_list(char *fname) ;
void read_vertex_list(char *fname) ;
void plot_nearest_nonzero(int vnum) ;
void find_nearest_nonzero(int vnum, int *indxptr) ;
void find_nearest_fixed(int vnum, int *vnumptr) ;
void plot_sol_time_course(int imin, int plot_xdim, int plot_ydim, 
                          int plot_xloc, int plot_yloc, int plotnum) ;
void load_vals_from_sol(float tmid, float dt, int num) ;
void load_var_from_sol(int num) ;
void variance_ratio(void) ;
void load_pvals(void) ;
void compute_pval_fwd(float pthresh) ;
void compute_select_fwd(float maxdist) ;
void compute_select_crosstalk(void) ;
void compute_all_crosstalk(int weighttype) ;
void compute_select_pointspread(void) ;
void compute_all_pointspread(void) ;
void recompute_select_inverse(void) ;
void compute_pval_inv(void) ;
void normalize_vals(void);
void normalize_time_courses(int normtype) ;
void normalize_inverse(void) ;
void setsize_window(int pix);
void resize_window(int pix) ;
void save_rgb(char *fname) ;
void save_rgb_num(char *dir) ;
void save_rgb_named_orig(char *dir, char *name) ;
void scrsave_to_rgb(char *fname) ;
void pix_to_rgb(char *fname) ;
void read_annotated_image(char *fpref, int frame_xdim, int frame_ydim) ;
void read_annotations(char *fname) ;
int diff(int a,int b) ;
void save_rgb_cmp_frame(char *dir, int ilat) ;
void open_rgb_cmp_named(char *dir, char *name) ;
void save_rgb_cmp_frame_named(float lat) ;
void do_rgb_cmp_frame(long xsize,long ysize, FILE *fp) ;
void close_rgb_cmp_named(void) ;
void redraw(void) ;
void twocond(int c0, int c1) ;
void redraw_second(void) ;
void blinkbuffers(void) ;
void redraw_overlay(void) ;
void draw_cursor(int vindex,int onoroff) ;
void draw_all_cursor(void) ;
void draw_all_vertex_cursor(void) ;
void clear_all_vertex_cursor(void) ;
void select_vertex(short sx,short sy) ;
void find_vertex_at_screen_point(short sx,short sy,int* ovno, float* od) ;
void invert_vertex(int vno) ;
void invert_face(int fno) ;
void orient_sphere(void) ;
void dump_vertex(int vno) ;
void dump_faces(int vno) ;
void left_click(short sx,short sy) ;
void sample_annotated_image(void) ;
void restore_zero_position(void) ;
void restore_initial_position(void) ;
void make_lateral_view(char *stem) ;
void make_lateral_view_second(char *stem) ;
void write_val_histogram(float min, float max, int nbins) ;
void write_view_matrix(char *dir) ;
void read_view_matrix(char *dir) ;
void translate_brain(float x, float y, float z) ;
void scale_brain(float s) ;
void rotate_brain(float a, char c) ;
void read_image_info(char *fpref) ;
void read_talairach(char *fname) ;
void read_images(char *fpref) ;
void alloc_curv_images(void) ;
void read_curv_images(char *fpref) ;
void curv_to_curvim(void) ;
void second_surface_curv_to_curvim(void) ;
void swap_curv(void);
void curvim_to_surface(void) ;
void curvim_to_second_surface(void) ;
void smooth_curvim(int window) ;
void add_subject_to_average_curvim(char *name, char *morphsubdir)   ;
void smooth_curvim_sparse(int niter) ;
unsigned char floattobyte(float f, float min, float max) ;
float bytetofloat(unsigned char c, float min, float max) ;
void write_images(unsigned char ***mat,char *fpref) ;
int  read_binary_surface(char *fname) ;
/* begin rkt */
int vset_read_vertex_set(int set, char* fname ) ;
int vset_set_current_set(int set) ;
/* end rkt */
void read_positions(char *name) ;
void save_surf(void) ;
void restore_surf(void) ;
void read_second_binary_surface(char *fname) ;
void read_second_binary_curvature(char *fname) ;
void normalize_second_binary_curvature(void) ;
void show_surf(char *surf_name) ;
int  read_orig_vertex_coordinates(char *fname) ;
int  read_inflated_vertex_coordinates(void) ;
int  read_white_vertex_coordinates(void) ;
int read_pial_vertex_coordinates(void) ;
int read_canon_vertex_coordinates(char *fname) ;
void send_spherical_point(char *subject_name,char *canon_name,
                          char *orig_fname);
void send_to_subject(char *subject_name) ;
static void resend_to_subject(void) ;
void invert_surface(void) ;
void read_ellipsoid_vertex_coordinates(char *fname,float a,float b,float c) ;
void find_orig_vertex_coordinates(int vindex) ;
void select_talairach_point(int *vindex,float x_tal,float y_tal,float z_tal);
void select_orig_vertex_coordinates(int *vindex) ;
void read_curvim_at_vertex(int vindex) ;
int  write_binary_surface(char *fname) ;
void write_binary_patch(char *fname) ;
void read_binary_patch(char *fname) ;
void write_labeled_vertices(char *fname) ;
void read_and_color_labeled_vertices(int r, int g, int b) ;
void read_labeled_vertices(char *fname) ;
void write_binary_curvature(char *fname) ;
void write_binary_areas(char *fname) ;
void write_binary_values(char *fname) ;
/* begin rkt */
void read_binary_values(char *fname) ;
void read_binary_values_frame(char *fname) ;
/* end rkt */
void swap_stat_val(void) ;
void swap_val_val2(void) ;
void shift_values(void) ;
void swap_values(void) ;
void read_binary_decimation(char *fname) ;
void write_binary_decimation(char *fname) ;
void write_decimation(char *fname) ;
void read_binary_dipoles(char *fname) ;
void write_binary_dipoles(char *fname) ;
void write_dipoles(char *fname) ;
void write_subsample(char *fname) ;
void write_binary_subsample(char *fname) ;
void read_binary_subsample(char *fname) ;
void read_binary_curvature(char *fname) ;
void normalize_binary_curvature(void) ;
void read_binary_areas(char *fname) ;
void read_fieldsign(char *fname);
void write_fieldsign(char *fname) ;
void read_fsmask(char *fname) ;
void write_fsmask(char *fname) ;
void write_vrml(char *fname,int mode) ;
void rip_faces(void) ;
void normalize(float v[3]) ;
void normal_face(int fac,int n,float *norm);
float triangle_area(int fac,int n) ;
void find_neighbors(void) ;
void compute_normals(void) ;
void compute_shear(void) ;
void double_angle(float x,float y,float *dx,float *dy) ;
void halve_angle(float x,float y,float *dx,float *dy);
void compute_boundary_normals(void) ;
void subsample_dist(int spacing) ;
void subsample_orient(float spacing) ;
void smooth_curv(int niter) ;
void smooth_val_sparse(int niter) ;
void smooth_val(int niter) ;
void smooth_fs(int niter) ;
void fatten_border(int niter) ;
void compute_angles(void) ;
float circsubtract(float a,float b) ;
void compute_fieldsign(void) ;
void compute_cortical_thickness(void) ;
static void read_disc(char *subject_name) ;
static void deconvolve_weights(char *weight_fname, char *scale_fname) ;
/* begin rkt */
void swap_vertex_fields(int a, int b); /* params are FIELD_*  */
static void print_vertex_data(int vno, FILE *fp, float dmin) ;
static void update_labels(int label_set, int vno, float dmin) ; /* LABELSET_ */

/* set the rip value of a face or vertex. */
void set_face_rip(int fno, int rip, int undoable);
void set_vertex_rip(int vno, int rip, int undoable);

/* adds another marked vertex to close the loop. */
void close_marked_vertices ();

/* draws a crosshair at a vertex in the current color. */
void draw_vertex_hilite (int vno);

/* draws all marked verts with white hilites, using draw_vertex_hilite(). */
void draw_marked_vertices ();
/* end rkt */

/* external prototypes */
void buffer_to_image(unsigned char *buf,unsigned char**im,int ysize,int xsize);
void image_to_buffer(unsigned char **im,unsigned char*buf,int ysize,int xsize);
void file_name(char *fpref,char *fname,int num,char *form) ;
void inverse(FLOATTYPE **a,FLOATTYPE **y, int n) ;
/* void vector_multiply(FLOATTYPE **a, FLOATTYPE *b, FLOATTYPE *c, int n, int m); */
void bpfilter(FLOATTYPE **data, int nchan, int nsamp,float lo,float hi);


/* begin rkt */

/* -------------------------------------------------- ctrl-c cancel support */

int cncl_listening = 0;
int cncl_canceled = 0;

void cncl_initialize ();
void cncl_start_listening ();
void cncl_stop_listening ();
int cncl_user_canceled ();

void cncl_handle_sigint (int signal);

/* ------------------------------------------------------------------------ */

/* ------------------------------------------------------- menu set support */

/* menu sets */
#define MENUSET_VSET_INFLATED_LOADED   0
#define MENUSET_VSET_WHITE_LOADED      1
#define MENUSET_VSET_PIAL_LOADED       2
#define MENUSET_VSET_ORIGINAL_LOADED   3
#define MENUSET_TIMECOURSE_LOADED      4
#define MENUSET_OVERLAY_LOADED         5
#define MENUSET_CURVATURE_LOADED       6
#define MENUSET_LABEL_LOADED           7

int enable_menu_set (int set, int enable);

/* ------------------------------------------------------------------------ */

/* -------------------------------------------- multiple vertex set support */

/*
 * this code lets the user load in alternate vertex sets and switch between
 * them. it doesn't use the vertices in the MRIS strucutre. each set is
 * loaded in with MRISread but then the vertices are copied into external
 * storage, leaaving the MRIS unchanged (except for the tmp vertices swap).
 * the main vertices are also copied into external storage. when the user
 * wants to display a different set, the vertices are copied from external
 * storage into the MRIS and the normals are recomputed.
 */

/* vertex sets */
#define VSET_MAIN      0
#define VSET_INFLATED  1
#define VSET_WHITE     2
#define VSET_PIAL      3
#define VSET_ORIGINAL  4
#define NUM_VERTEX_SETS  5

/* the current set */
static int vset_current_set = VSET_MAIN ;

/* storage for other sets, a node and a list. the list is allocated
   dynamically, getting the number of verts from the MRIS. */
typedef struct {
  float x,y,z;
} VSET_VERTEX;

VSET_VERTEX* vset_vertex_list[NUM_VERTEX_SETS];

/* call at startup. sets the storage pointers to null. */
int vset_initialize ();

/* reads in a vertex set from a file. actually copies the MRIS main verts
   into tmp space in the MRIS, loads the surface, copies the verts into
   external storage, and restores the MRIS verts from the tmp storage. */
int vset_read_vertex_set(int set, char* fname);

/* copies and restores main verts from MRIS into and out of 
   external storage. */
int vset_save_surface_vertices(int set);
int vset_load_surface_vertices(int set);

/* changes the current set, calling vset_load_surface_vertices and recomputes
   the normals. marks the vertex array dirty for the next redraw. */
int vset_set_current_set(int set);

/* ------------------------------------------------------------------------ */

/* -------------------------------------------------- coordinate conversion */

MATRIX* conv_mnital_to_tal_m_ltz = NULL;
MATRIX* conv_mnital_to_tal_m_gtz = NULL;
MATRIX* conv_tmp1_m = NULL;
MATRIX* conv_tmp2_m = NULL;

int conv_initialize ();
int conv_mnital_to_tal(float mnix, float mniy, float mniz,
           float* talx, float* taly, float* talz);

/* ------------------------------------------------------------------------ */

/* ----------------------------------------------------------- undo support */

#include "xGrowableArray.h"

/* 
 * an undo action (UNDO_ACTION) represents a single logical action that can
 * be undone in user langugage, such as a cut that effected many verices
 * and faces. an action has a type and a list of action nodes. the type
 * represents that type of action that can be undone (i.e. UNDO_CUT), and has
 * an associated node type (i.e. UNDO_CUT_NODE).
 *
 * to start an undoable action, call undo_begin_action to allocate the
 * action. then pass pointers to the action node to undo_copy_action_node,
 * in which it will be copied to the list. when done adding nodes, call
 * undo_finish_action.
 *
 * use the undo_get_action_string to get a human readable description of the
 * action to be undo. call undo_do_first_action to undo the last action. multiple
 * undos can be done, up to NUM_UNDOS. undoing an action removes the action
 * from the undo list - it's not 'redoable'.
 */

/* types of undo actions */
#define UNDO_INVALID      0
#define UNDO_NONE         1
#define UNDO_CUT          2
#define NUM_UNDO_ACTIONS  3

char undo_action_strings[NUM_UNDO_ACTIONS][NAME_LENGTH] = 
{
  "INVALID UNDO ACTION",
  "Nothing to Undo",
  "Undo Cut" 
};

/* represents an undoable action, a list of action nodes of specific types */
typedef struct 
{
  int undo_type; /* UNDO_* */
  xGrowableArrayRef node_list;
} UNDO_ACTION;

/* storage of undo actions. when the list is open, undo_list[0] is the list 
   being created and added to. when closed, undo_list[0] is the first
   action that will be undone. */
#define UNDO_LIST_POS_FIRST 0
#define UNDO_LIST_POS_LAST  3
#define NUM_UNDO_LISTS      4
static UNDO_ACTION* undo_list[NUM_UNDO_LISTS];

/* initialize anything that needs to get initialized */
int undo_initialize ();

/* gets the size of the node associated with this action type. returns -1
   if the action type is invalid. */
int undo_get_action_node_size(int action_type); /* UNDO_* */

/* return the string that describes the action available to be undone.
   returns a null string if the action type is invalid. */
char* undo_get_action_string(int action_type); /* UNDO_* */

/* creates a new undo list and inserts it in the undo_list array. can possibly
   remove and delete an action if the list is full. add_undoable_action_node
   can be called to add individual action nodes. returns en error if the
   action is already begun. */
int undo_begin_action(int action_type);

/* finishes the list creation. */
int undo_finish_action();

/* sends a message to tcl setting the undo menu item. */
int undo_send_first_action_name();

/* undo_begin_action sets to open, undo_finish_action sets to
   closed. shouldn't be set directly. */
#define UNDO_LIST_STATE_CLOSED    0
#define UNDO_LIST_STATE_OPEN      1
static int undo_list_state=UNDO_LIST_STATE_CLOSED;

/* copies the node tothe list being created. returns an error if the list
   is closed. */
int undo_copy_action_node(void* node);

/* undoes the first action if the list is closed. returns an error if the
   list is open. */
int undo_do_first_action();

/* UNDO_CUT */
/* type of cut node */
#define UNDO_CUT_VERTEX 0
#define UNDO_CUT_FACE   1

typedef struct 
{
  int cut_type;   /* UNDO_CUT_* */
  int index;      /* face or vertex */
  int rip_value;  /* the value that will be restored */
} UNDO_CUT_NODE;

/* creates an undo cut node add and copies it to the current open list. */
int undo_new_action_cut(int cut_type, int index, int rip_value);

/* undoes an action node, i.e. sets the referenced face's or vertex's 
   rip value to the value in the node. */
int undo_do_action_cut(UNDO_ACTION* action);

/* ---------------------------------------------------------------------- */

/* -------------------------------------------- functional volume support */

#include "mriFunctionalDataAccess.h"
#include "xUtilities.h"

/* we keep a separate list of  */
typedef struct
{
  float x, y, z;
} FUNC_SELECTED_VOXEL;

static mriFunctionalDataRef func_timecourse = NULL;
static mriFunctionalDataRef func_timecourse_offset = NULL;
static int func_use_timecourse_offset = FALSE;
static int func_sub_prestim_avg = FALSE;
static xGrowableArrayRef func_selected_ras = NULL;

#define knLengthOfGraphDataItem              18 // for "100.1 1000.12345 "
#define knLengthOfGraphDataHeader            20 // for cmd name + cond + {}
#define knMaxCommandLength                   50

/* tcl vars */
static double func_time_resolution = 0;
static int func_num_prestim_points = 0;
static int func_num_conditions = 0;

int func_initialize ();

int func_load_timecourse (char* dir, char* stem, char* registration);
int func_load_timecourse_offset (char* dir, char* stem, char* registration);

int func_select_marked_vertices ();
int func_select_label ();

int func_clear_selection();
int func_select_ras (float x, float y, float z);

int func_graph_timecourse_selection ();

int func_print_timecourse_selection (char* filename);

int func_calc_avg_timecourse_values (int condition, int* num_good_voxels,
             float values[], float deviations[] );

int func_convert_error (FunD_tErr error);

/* ---------------------------------------------------------------------- */

/* --------------------------------------------------- scalar value mgmnt */

#define SCLV_VAL          0
#define SCLV_VAL2         1
#define SCLV_VALBAK       2
#define SCLV_VAL2BAK      3
#define SCLV_VALSTAT      4
#define SCLV_IMAG_VAL     5
#define SCLV_MEAN         6
#define SCLV_MEAN_IMAG    7
#define SCLV_STD_ERROR    8
#define NUM_SCALAR_VALUES 9

#if 0
#define SCLV_FIELDSIGN    6
#define SCLV_FSMASK       7
#endif

static char sclv_field_names [NUM_SCALAR_VALUES][255] = {
  "val", "val2", "valbak", "val2bak", "valstat", "imagval",
  "mean", "meanimag", "std_error"};

typedef struct {
  int is_binary_volume;
  int cur_timepoint;
  int cur_condition;
  int num_timepoints; /* always 1 for .w files */
  int num_conditions; /* always 1 for .w files */
  mriFunctionalDataRef volume;
  float fthresh;
  float fmid;
  float fslope;
  float foffset;
  float min_value;
  float max_value;
  int ***frequencies; /* the frequency of values in num_freq_bins for
       each time point and condition i.e.
       frequency[cond][tp][bin] */
  int num_freq_bins;
} SCLV_FIELD_INFO;

static int sclv_current_field = SCLV_VAL;
static SCLV_FIELD_INFO sclv_field_info[NUM_SCALAR_VALUES]; 
static double sclv_value_min = 0;
static double sclv_value_max = 0;
static int sclv_num_timepoints = 0;
static int sclv_num_conditions = 0;
static int sclv_cur_timepoint = 0;
static int sclv_cur_condition = 0;

#define sclv_set_value(v,i,n) \
 switch(i) { \
     case SCLV_VAL: (v)->val = n; break; \
     case SCLV_VAL2: (v)->val2 = n; break; \
     case SCLV_VALBAK: (v)->valbak = n; break; \
     case SCLV_VAL2BAK: (v)->val2bak = n; break; \
     case SCLV_VALSTAT: (v)->stat = n; break; \
     case SCLV_IMAG_VAL: (v)->imag_val = n; break; \
     case SCLV_MEAN: (v)->mean = n; break; \
     case SCLV_MEAN_IMAG: (v)->mean_imag = n; break; \
     case SCLV_STD_ERROR: (v)->std_error = n; break; \
 }

#define sclv_get_value(v,i,n) \
 switch(i) { \
     case SCLV_VAL: (*n) = (v)->val; break; \
     case SCLV_VAL2: (*n) = (v)->val2; break; \
     case SCLV_VALBAK: (*n) = (v)->valbak; break; \
     case SCLV_VAL2BAK: (*n) = (v)->val2bak; break; \
     case SCLV_VALSTAT: (*n) = (v)->stat; break; \
     case SCLV_IMAG_VAL: (*n) = (v)->imag_val; break; \
     case SCLV_MEAN: (*n) = (v)->mean ; break; \
     case SCLV_MEAN_IMAG: (*n) = (v)->mean_imag; break; \
     case SCLV_STD_ERROR: (*n) = (v)->std_error; break; \
 }

int sclv_initialize ();

int sclv_unload_field (int field);

int sclv_read_binary_values (char* fname, int field);
int sclv_read_binary_values_frame (char* fname, int field);

int sclv_read_bfile_values (char* dir, char* stem, 
          char* registration, int field);

/* writes .w files only */
int sclv_write_binary_values (char* fname, int field);

/* make a pass through all the conditions and timepoints and calculate
   frequencies for everything. */
#define SCLV_NUM_FREQUENCY_BINS 250
int sclv_calc_frequencies (int field);

/* generic version of smooth_val, smooth_fs, etc */
int sclv_smooth (int niter, int field);

int sclv_set_current_field (int field);
int sclv_send_current_field_info ();

/* sets the timepoint of an overlay. if this is a binary volume, this
   will actually paint the timepoint's data onto the proper sclv field
   in the volume.*/
int sclv_set_timepoint_of_field (int field, int timepoint, int condition);

/* this stuff is kind of experimental, it works but it's not very
   useful as the percentages have to be very precise. */
int sclv_set_current_threshold_from_percentile (float thresh, float mid, 
            float max);
int sclv_set_threshold_from_percentile         (int field, float thresh,
            float mid, float max);
int sclv_get_value_for_percentile              (int field, float percentile, 
            float* value);

/* copies field settings from one field to another */
int sclv_copy_view_settings_from_current_field (int field);
int sclv_copy_view_settings_from_field (int field, int fromfield);

/* swaps values in two fields */
int sclv_swap_fields (int fielda, int fieldb);

/* sends a tcl command with the information necessary to build a
   histogram. */
int sclv_send_histogram ( int field );

/* gets a single 0-1 color value for a scalar value. used by tcl to
   color a histogram. */
int sclv_get_normalized_color_for_value (int field, float value, 
           float *outRed,
           float *outGreen,
           float *outBlue);


/* ---------------------------------------------------------------------- */

/* ------------------------------------------------------ multiple labels */

#include "mriColorLookupTable.h"

int labl_debug = 0;

typedef enum {
  LABL_STYLE_OPAQUE = 0,
  LABL_STYLE_OUTLINE,
  LABL_NUM_STYLES
} LABL_DRAW_STYLE;

/* a label without an assigned structure. */
#define LABL_TYPE_FREE -1

/* the default color for free labels. */
#define LABL_DEFAULT_COLOR_R 100
#define LABL_DEFAULT_COLOR_G 100
#define LABL_DEFAULT_COLOR_B 200

typedef struct 
{
  LABEL* label;
  int structure;    /* the structure assigned to this label, or
           LABL_TYPE_FREE for a free color label */
  int r, g, b;      /* from color of structure in lookup table or if
           LABL_TYPE_FREE, assigned by the user */
  int visible;
  char name[NAME_LENGTH];  /* name of this label (not neccessarily the
            same as the structure name, if
            assigned) */
  float min_x, max_x;    /* the bounding cube of this label. */
  float min_y, max_y;
  float min_z, max_z;
} LABL_LABEL;

/* macro to pack and unpack an annotation value into rgb values */
#define LABL_UNPACK_ANNOT(annot,r,g,b)   \
    r = annot & 0xff ;         \
    g = (annot >> 8) & 0xff ;  \
    b = (annot >> 16) & 0xff ; \

int labl_tmp_r, labl_tmp_g, labl_tmp_b;
#define LABL_PACK_ANNOT(r,g,b,annot) \
      labl_tmp_r = (r) & 0xff;           \
      labl_tmp_g = ((g) & 0xff) << 8;    \
      labl_tmp_b = ((b) & 0xff) << 16;   \
      annot = labl_tmp_r | labl_tmp_g | labl_tmp_b;

/* a fixed array of labels. */
#define LABL_MAX_LABELS 300
LABL_LABEL labl_labels[LABL_MAX_LABELS]; /* oh, for the c++ stl.. */
int labl_num_labels;

/* the color lookup table. */
mriColorLookupTableRef labl_table = NULL;
int labl_num_structures;

/* the currently selected label */
#define LABL_NONE_SELECTED -1
int labl_selected_label = LABL_NONE_SELECTED;

/* a global count of labels crated for naming purposes. */
int labl_num_labels_created;

/* style in which to draw the labels. */
LABL_DRAW_STYLE labl_draw_style = LABL_STYLE_OPAQUE;

/* array of flags for each mris vertex, whether or not a surface
   vertex is a label border. 0 is not a border, 1 is single width
   border, 2 is double width border. */
short* labl_is_border = NULL;

/* array of label indices for each mris vertex. */
int* labl_top_label = NULL;

/* wheter or not the label cache is current. */
char labl_cache_is_current = 0;

/* whether or not to draw labels. */
int labl_draw_flag = 1;

/* the fudge for the bounding cube for each label. */
#define LABL_FUDGE 4.0

/* name of the color table. */
char* labl_color_table_name;

/* initialize everything. */
int labl_initialize ();

/* loads a new color table. if any labels had existing indicies that
   are now out of bounds, they are set to 0. sends an update to the
   tcl list with the structure names. */
int labl_load_color_table (char* fname);

/* NOTE: read_labeled_vertices now routed here. */
int labl_load (char* fname);
int labl_save (int index, char* fname);
int labl_save_all (char* prefix);

/* finds the border of a label and sets the border flags appropriately. */
int labl_find_and_set_border (int index);

/* clears the border flags for a label. */
int labl_clear_border (int index);

/* finds the top label for each vertex and caches the value. */
int labl_update_label_cache ();

/* reads an annotation file and makes multiple labels, one for each
   annotation. tries to associate a structure index by matching the
   color with a structure in the current lookup table. otherwise marks
   it 0. NOTE: read_annotations now routed here. */
int labl_import_annotation (char* fname);

/* saves all labels in one annotation file. if any labels have shared
   voxels, the last label in the list will overwrite any previous
   ones. */
int labl_export_annotation (char* fname);

/* makes the marked vertices a new label. */
int labl_new_from_marked_vertices ();

/* marks all the vertices in a label. */
int labl_mark_vertices (int index);

/* selects a label, drawing it with apn outline and hilighting it in
   the label list tcl window. */
int labl_select (int index);

/* sets the label index name from the structure name in the color table */
int labl_set_name_from_table (int index);

/* changes information about a label. */
int labl_set_info (int index, char* name, int structure, int visible,
       int r, int g, int b);
/* changes the color of a label. only valid if it is a free label. */
int labl_set_color (int index, int r, int g, int b);

/* sends a label's information to tcl */
int labl_send_info (int index);

/* makes a new LABL_LABEL entry with this label data. note that this
   function uses the label you pass in, so don't delete it. passes
   back the index of the label. sends an update to the tcl list with
   the new name. */
int labl_add (LABEL* label, int* new_index);

/* removes and deletes the label and bumps down all other labels in
   the list. */
int labl_remove (int index);

/* removes and deletes all labels. */
int labl_remove_all ();

/* checks if this vno is in a label. passes back the label index. */
int labl_find_label_by_vno (int vno, int* index);

/* figures out if a click is in a label. if so, selects it. */
int labl_select_label_by_vno (int vno);

/* if this vno is in a label, changes the color of this vertex
   accordingly. */
int labl_apply_color_to_vertex (int vno, GLubyte* r, GLubyte* g, GLubyte* b );

/* prints the label list. */
int labl_print_list ();

/* prints the color table. */
int labl_print_table ();

/* ---------------------------------------------------------------------- */

/* ------------------------------------------------------ fill boundaries */

int fbnd_debug = 0;

typedef struct
{
  int num_vertices;    /* number of vertices in line. */
  int* vertices;    /* array of vertices in line. */
  float min_x, max_x;    /* the bounding cube of this, uh, boundary. */
  float min_y, max_y;
  float min_z, max_z;
  
} FBND_BOUNDARY;

/* array of boundaries. */
#define FBND_MAX_BOUNDARIES 200
FBND_BOUNDARY fbnd_boundaries[FBND_MAX_BOUNDARIES];
int fbnd_num_boundaries = 0;

/* currently selected boundary. */
#define FBND_NONE_SELECTED -1
int fbnd_selected_boundary = FBND_NONE_SELECTED;

static FBND_BOUNDARY *fbnd_copy(FBND_BOUNDARY *bsrc, FBND_BOUNDARY *bdst) ;
static int           fbnd_set_marks(FBND_BOUNDARY *b, MRI_SURFACE *mris,int mark) ;
static double        fbnd_sse(FBND_BOUNDARY *b, MRI_SURFACE *mris, double target_curv, 
                              double l_curv, double l_len) ;
static double        fbnd_length(FBND_BOUNDARY *b, MRI_SURFACE *mris) ;

#define FBND_FUDGE 4.0

/* array of flags for each mris vertex, whether or not a surface
   vertex is in a line. */
char* fbnd_is_boundary = NULL;

int fbnd_initialize ();

int fbnd_new_line_from_marked_vertices ();

int fbnd_remove_selected_boundary ();

int fbnd_add (int num_vertices, int* vertices, int* new_index);
int fbnd_remove (int index);

int fbnd_select (int index);

/* updates the fbnd_is_boundary array after a boundary has been added
   or removed. */
int fbnd_update_surface_boundaries ();

/* returns true if this vertex is on a fill boundary. */
char fbnd_is_vertex_on_boundary (int vno);

/* figures out if a click is on or near a boundary. if so, selects it. */
int fbnd_select_boundary_by_vno (int vno);

/* if this vno is a boundary, changes the color of this vertex
   accordingly. */
int fbnd_apply_color_to_vertex (int vno, GLubyte* r, GLubyte* g, GLubyte* b );

/* ---------------------------------------------------------------------- */

/* ------------------------------------------------------- floodfill mark */

typedef struct {
  
  char dont_cross_boundary;
  char dont_cross_label;
  char dont_cross_cmid;
  char dont_cross_fthresh;
  
} FILL_PARAMETERS;

int fill_flood_from_seed (int vno, FILL_PARAMETERS* params);

/* ---------------------------------------------------------------------- */

/* end rkt */


#ifdef TCL
int Surfer(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
#else
     int  mai(int argc,char *argv[])
#endif
{
  int tclscriptflag;
  int option=0;
  int i;
  int j;
  long last_frame_xdim;
  FILE *fplog, *pplog;
  char str[NAME_LENGTH];
  char logbuf[NAME_LENGTH];
  char lsubjectsdir[NAME_LENGTH];
  char lsrname[NAME_LENGTH];
  char lpname[NAME_LENGTH];
  char lstem[NAME_LENGTH];
  char lext[NAME_LENGTH];
  char *envptr;
  char cwd[100*NAME_LENGTH];
  char *word;
  char path[MAX_DIR_DEPTH][NAME_LENGTH];
  int  nargs ;
  char *functional_path = NULL, *functional_stem = NULL, *patch_name = NULL ;
  /* begin rkt */
  char timecourse_path_and_stem[NAME_LENGTH];
  int load_timecourse = FALSE;
  int use_timecourse_reg = FALSE;
  char timecourse_offset_path_and_stem[NAME_LENGTH];
  int load_timecourse_offset = FALSE;
  char timecourse_path[NAME_LENGTH];
  char timecourse_stem[NAME_LENGTH];
  char timecourse_reg[NAME_LENGTH];
  char timecourse_offset_path[NAME_LENGTH];
  char timecourse_offset_stem[NAME_LENGTH];
  xUtil_tErr util_error;
  /* end rkt */
  
  InitDebugging("tksurfer") ;
  EnableDebuggingOutput ;
  
  for (i = 0 ; i < argc ; i++)
    {
      /*      fprintf(stderr, "argv[%d] = %s\n", i, argv[i]);*/
      if (!stricmp(argv[i], "-o")) 
  {
    nargs = 3 ;
    functional_path = argv[i+1] ;
    functional_stem = argv[i+2] ;
  }
      else if (!stricmp(argv[i], "-fslope"))
  {
    nargs = 2 ;
    fslope = atof(argv[i+1]) ;
    fprintf(stderr, "setting fslope to %2.4f\n", fslope) ;
  }
      else if (!stricmp(argv[i], "-colscalebarflag"))
  {
    nargs = 2 ;
    colscalebarflag = atoi(argv[i+1]) ;
    fprintf(stderr, "setting colscalebarflag to %d\n", colscalebarflag) ;
  }
      else if (!stricmp(argv[i], "-scalebarflag"))
  {
    nargs = 2 ;
    scalebarflag = atoi(argv[i+1]) ;
    fprintf(stderr, "setting scalebarflag to %d\n", scalebarflag) ;
  }
      else if (!stricmp(argv[i], "-truncphaseflag"))
  {
    nargs = 2 ;
    truncphaseflag = atoi(argv[i+1]) ;
    fprintf(stderr, "setting truncphaseflag to %d\n", truncphaseflag) ;
  }
      else if (!stricmp(argv[i], "-revphaseflag"))
  {
    nargs = 2 ;
    revphaseflag = atoi(argv[i+1]) ;
    fprintf(stderr, "setting revphaseflag to %d\n", revphaseflag) ;
  }
      else if (!stricmp(argv[i], "-invphaseflag"))
  {
    nargs = 2 ;
    invphaseflag = atoi(argv[i+1]) ;
    fprintf(stderr, "setting invphaseflag to %d\n", invphaseflag) ;
  }
      else if (!stricmp(argv[i], "-fthresh"))
  {
    nargs = 2 ;
    fthresh = atof(argv[i+1]) ;
    fprintf(stderr, "setting fthresh to %2.4f\n", fthresh) ;
  }
      else if (!stricmp(argv[i], "-patch"))
  {
    nargs = 2 ;
    patch_name = argv[i+1] ;
    fprintf(stderr, "displaying patch %s...\n", patch_name) ;
  }
      else if (!stricmp(argv[i], "-fmid"))
  {
    nargs = 2 ;
    fmid = atof(argv[i+1]) ;
    fprintf(stderr, "setting fmid to %2.4f\n", fmid) ;
  }
      else if (!stricmp(argv[i], "-foffset"))
  {
    nargs = 2 ;
    foffset = atof(argv[i+1]) ;
    fprintf(stderr, "setting foffset to %2.4f\n", foffset) ;
  }
      else if (!stricmp(argv[i], "-sdir"))
  {
    nargs = 2 ;
    sdir = argv[i+1] ;
    fprintf(stderr, "using SUBJECTS_DIR %s\n", sdir) ;
  }
      else if (!stricmp(argv[i], "-orig"))
  {
    nargs = 2 ;
    orig_suffix = argv[i+1] ;
    fprintf(stderr, "using orig suffix %s\n", orig_suffix) ;
  }
      /* begin rkt */
      else if (!stricmp(argv[i], "-timecourse"))
  {
    nargs = 2;
    strncpy (timecourse_path_and_stem, argv[i+1], sizeof(timecourse_path_and_stem));
    load_timecourse = TRUE;
  }
      else if (!stricmp(argv[i], "-timecourse-reg"))
  {
    nargs = 2;
    strncpy (timecourse_reg, argv[i+1], sizeof(timecourse_reg));
    use_timecourse_reg = TRUE;
  }
      else if (!stricmp(argv[i], "-timecourse-offset"))
  {
    nargs = 2;
    strncpy (timecourse_offset_path_and_stem, argv[i+1], sizeof(timecourse_offset_path_and_stem));
    load_timecourse_offset = TRUE;
  }
      /* end rkt */
      else
        nargs = 0 ;
      if (nargs > 0)
  {
    if (argc-(nargs+i) > 0)
      memmove(argv+i, argv+i+nargs, (argc-(nargs+i))*sizeof(char*)) ;
    argc -= nargs ; i-- ;
  }
    }
  
  /* args */
  if (argc==2)
    strcpy(str,argv[1]);
  if (MATCH_STR("-help") || MATCH_STR("-h")) {
    strcpy(str,argv[0]);
    if (MATCH_STR("surfer")) print_help_surfer();
    if (MATCH_STR("tksurfer")) print_help_tksurfer();
    exit(1);
  }
  if (argc<4 || argc>6) {
    strcpy(str,argv[0]);
    if (MATCH_STR("surfer")) {
      printf("\n");
      printf("Usage: %s [-]name hemi surf\n",argv[0]);
      printf("       %s -help, -h [after startup: h,?]\n",argv[0]);
      printf("\n");
    }
    if (MATCH_STR("tksurfer")) {
      printf("\n");
      printf("Usage: %s [-]name hemi surf [-tcl script]\n",argv[0]);
      printf("       %s -help, -h [after startup: help]\n",argv[0]);
      printf("\n");
      printf("   name:  subjname (dash prefix => don't read MRI images)\n");
      printf("   hemi:  lh,rh\n");
      printf("   surf:  orig,smoothwm,plump,1000,1000a\n");
      printf("\n");
    }
#ifdef OPENGL
    printf("                                    [vers: 24m23s23j--OpenGL]\n");
#else
    printf("                                    [vers: 24m23s23j--GL]\n");
#endif
    exit(1);
  }
  if (argv[1][0]=='-') {
    MRIflag = FALSE;
    MRIloaded = FALSE;
    sprintf(lpname,"%s",argv[1]+1);
  }
  else {
#if 0
    MRIflag = TRUE;
    MRIloaded = TRUE;
#else
    MRIflag = FALSE;    /* don't load MRI volume - BRF */
    MRIloaded = FALSE;
#endif
    sprintf(lpname,"%s",argv[1]);
  }
  sprintf(lstem,"%s",argv[2]);
  sprintf(lext,"%s",argv[3]);
  tclscriptflag = FALSE;
  if (argc>=5) {
    strcpy(str,argv[4]);
    if (MATCH_STR("-tcl"))
      tclscriptflag = TRUE;
    else {
      option = atoi(argv[4]);
      if (option==5) {
  printf("surfer: ### option 5 defunct--instead use:\n\n");
  printf("    tksurfer %s %s %s -tcl inflate.tcl\n\n",
         argv[1],argv[2],argv[3]);
      }
      else if (option==12) {
  printf("surfer: ### option 12 defunct--instead use:\n\n");
  printf("    tksurfer %s %s %s -tcl flatten.tcl\n\n",
         argv[1],argv[2],argv[3]);
      }
      else {
  printf("surfer: defunct option\n");
      }
      exit(1);
    }
  }
  
  /* subjects dir from environment */
  if (sdir)
    envptr = sdir ;
  else
    envptr = getenv("SUBJECTS_DIR");
  if (envptr==NULL) {
    printf("surfer: env var SUBJECTS_DIR undefined (use setenv)\n");
    exit(1);
  }
  strcpy(lsubjectsdir,envptr);
  printf("surfer: current subjects dir: %s\n",lsubjectsdir);
  
  /* guess datadir from cwd path */
  getwd(cwd);
  word = strtok(cwd,"/");
  strcpy(path[0],word);
  i = 1; 
  while ((word = strtok(NULL,"/")) != NULL) {  /* save,count */
    strcpy(path[i],word); 
    i++; 
  }
  if (MATCH(path[i-1],"scripts") && MATCH(path[i-2],"image")) {
    printf("surfer: in new (APD2) format \"scripts\" dir\n");
    j = i-1;
  } else if (MATCH(path[i-1],"scripts") && strspn(path[i-2],"0123456789")==5){
    printf("surfer: in old (APD1) format \"scripts\" dir\n");
    j = i-1;
  } else if (MATCH(path[i-1],"scripts") && MATCH(path[i-2],lpname)){
    printf("surfer: in subjects \"scripts\" dir\n");
    j = i-1;
  } else if (MATCH(path[i-1],"scripts")) {
    printf("surfer: in \"scripts\" dir (not APD1,APD2,subjects format)\n");
    j = i-1;
  } else if (strstr(path[i-1],"scripts")!=NULL) {
    printf("surfer: in dir with \"scripts\" in name\n");
    j = i-1;
  } else {
    printf("surfer: not in \"scripts\" dir ==> using cwd for session root\n");
    j = i;
  }
  sprintf(lsrname,"/%s",path[0]);  /* reassemble absolute */
  for(i=1;i<j;i++)
    sprintf(lsrname,"%s/%s",lsrname,path[i]);
  printf("surfer: session root data dir ($session) set to:\n");
  printf("surfer:     %s\n",lsrname);
  
  /* logfile */
  fplog = fopen("surfer.log","a");
  if (fplog==NULL) {
    printf("surfer: can't create file surfer.log in cwd\n");
    sprintf(str,"/tmp/surfer.log.%s",getenv("USER"));
    fplog = fopen(str,"a");
    if (fplog==NULL) {
      printf("surfer: can't create surfer.log--written to stdout\n");
      fplog = stdout;  /* give up and write log to standard out */
    }
    else {
      printf("surfer: surfer.log created in /tmp\n");
      strcpy(lsrname,"/tmp");
      printf("surfer: session root data dir ($session) reset to:\n");
      printf("surfer:     %s\n",lsrname);
    }
  }
  fprintf(fplog,"\n\n############################\n");
  pplog = popen("date","r");    /* date */
  fgets(logbuf,NAME_LENGTH,pplog);
  fprintf(fplog,"%s",logbuf);
  pclose(pplog);
  pplog = popen("pwd","r");     /* pwd */
  fgets(logbuf,NAME_LENGTH,pplog);
  fprintf(fplog,"%s",logbuf);
  pclose(pplog);
  for(i=0;i<argc;i++)           /* argv */
    fprintf(fplog,"%s ",argv[i]);
  fprintf(fplog,"\n############################\n");
  fflush(fplog);
  
  framebuff = (unsigned long *)calloc(frame_xdim*frame_ydim,sizeof(long));
  framebuff2 = (unsigned long *)calloc(frame_xdim*frame_ydim,sizeof(long));
  framebuff3 = (unsigned long *)calloc(frame_xdim*frame_ydim,sizeof(long));
  binbuff = (unsigned char *)calloc(3*frame_xdim*frame_ydim,sizeof(char));
  last_frame_xdim = frame_xdim;
  sf=0.55;
  
  make_filenames(lsubjectsdir,lsrname,lpname,lstem,lext);
  
  if (MATCH(cwd,srname)) {   /* if elsewhere, write rgbs in cwd */
    strcpy(gfname,srname);
    strcpy(sgfname,srname);
    sprintf(agfname,"%s/%s",srname,"surfer.rgb");
    printf("surfer: all rgb files written to %s\n",srname);
  }
  
  read_image_info(mfname);
  
  xmin = xx0; xmax = xx1;
  ymin = yy0; ymax = yy1;
  zmin = zz0; zmax = zz1;
  
  read_talairach(xffname);
  
  if (MRIflag) read_images(mfname);
  
  if (read_binary_surface(ifname) != NO_ERROR)
    ErrorExit(Gerror, "%s: could not read surface file %s.",Progname,ifname);
  
  if (read_orig_vertex_coordinates(orfname) == NO_ERROR)
    {
      char fname[STRLEN] ;
      sprintf(fname, "%s.sphere.reg", fpref) ;
      if (FileExists(fname))
        read_canon_vertex_coordinates(fname) ;
    }
  if (functional_path)  /* -o specified on command line */
    {
      char fname[STRLEN] ;
      
      sprintf(fname, "%s/%s", functional_path, functional_stem) ;
      read_binary_values(fname) ;
      read_binary_curvature(cfname) ; val_to_stat() ;
      overlayflag = TRUE ;
      colscale = HEAT_SCALE ;
    }
#if 0
  read_binary_areas(afname);
#endif
  
  /* begin rkt */
  if (load_timecourse)
    {
      
      util_error=xUtil_BreakStringIntoPathAndStem (timecourse_path_and_stem,
               timecourse_path, timecourse_stem);
      if (util_error!=xUtil_tErr_NoError)
  ErrorPrintf(0,"Couldn't load time course: file not found?\n");
      else {
  if (use_timecourse_reg)
    func_load_timecourse (timecourse_path, 
        timecourse_stem, timecourse_reg);
  else
    func_load_timecourse (timecourse_path, timecourse_stem, NULL);
      }
    }
  
  if (load_timecourse_offset)
    {
      
      util_error = xUtil_BreakStringIntoPathAndStem 
  (timecourse_offset_path_and_stem, 
   timecourse_offset_path, timecourse_offset_stem);
      if (util_error!=xUtil_tErr_NoError)
  ErrorPrintf(0,"Couldn't load time course: file not found?\n");
      else {
  if (use_timecourse_reg)
    func_load_timecourse_offset (timecourse_offset_path, 
               timecourse_offset_stem,timecourse_reg);
  else
    func_load_timecourse_offset (timecourse_offset_path,
               timecourse_offset_stem, NULL);
      }
    }
  
  /* end rkt */
  
  if (tclscriptflag) {  /* tksurfer tcl script */
    /* called from tksurfer.c; do nothing (don't even open gl window) */
    /* wait for tcl interp to start; tksurfer calls tcl script */
  }
  else {   /* open window for surfer or non-script tksurfer (a few envs) */
#ifndef Linux
    if ((envptr=getenv("doublebufferflag"))!=NULL) { /*tmp:TODO OGL toggle*/
      if (MATCH("1",envptr))     doublebufferflag = TRUE;
      if (MATCH("TRUE",envptr))  doublebufferflag = TRUE;
    }
    if ((envptr=getenv("renderoffscreen"))!=NULL) {
      if (MATCH("1",envptr))     renderoffscreen = TRUE;
      if (MATCH("TRUE",envptr))  renderoffscreen = TRUE;
    }
#endif
    open_window(pname);
    if (stem[0]=='r'&&stem[1]=='h')
      rotate_brain(-90.0,'y');
    else
      rotate_brain(90.0,'y');
    redraw();
  }
  
#ifndef TCL  /* tcl: omit event loop */
  if (tclscriptflag) {
    printf("surfer: can't read tcl script  ...ignored (use tksurfer)\n");
  }
#ifdef OPENGL
  printf("surfer: ### non-tk surfer event loop not converted for OpenGL\n");
  exit(1); 
#else
  while(1)  /* event loop */
    {
      {
  dev = qread(&val);  /* waits here for next event */
  switch(dev)  {
  case ESCKEY:       /* kill on escape */
    fflush(fplog);
    fclose(fplog);
    if (openvalfileflag) fclose(fpvalfile);
    system("rm -f /tmp/SurferTmp");
    system("rm -f /tmp/SurferLoop");
    exit(0);
    break;
  case LEFTMOUSE:
    if (val == 0)  break;
    sx = getvaluator(MOUSEX);
    sy = getvaluator(MOUSEY);
    if (ctrlkeypressed) { /* scale around click */
      getorigin(&ox,&oy);
      getsize(&lx,&ly);
      wx = sf*(sx-ox-lx/2.0)*2.0*fov/lx;
      wy = sf*(sy-oy-ly/2.0)*2.0*fov/ly;
      translate_brain(-wx,-wy,0.0);
      fprintf(fplog,"translate_brain_x %f\n",wx);
      fprintf(fplog,"translate_brain_y %f\n",wy);
      if (shiftkeypressed)
        scale_brain(3*SCALE_UP_MOUSE);
      else
        scale_brain(SCALE_UP_MOUSE);
      fprintf(fplog,"scale_brain %2.3f\n",SCALE_UP_MOUSE);
      redraw();
    }
    else {
      if (selection>=0)
        draw_cursor(selection,FALSE);
      select_vertex(sx,sy);
      if (selection>=0)
        mark_vertex(selection,TRUE);
      if (selection>=0)
        draw_cursor(selection,TRUE);
      fprintf(fplog,"#mark_vertex xpix=%d ypix=%d\n",sx,sy);
    }
    break;
  case MIDDLEMOUSE:
    if (val == 0)  break;
    sx = getvaluator(MOUSEX);
    sy = getvaluator(MOUSEY);
    select_vertex(sx,sy);
    if (selection>=0)
      mark_vertex(selection,FALSE);
    fprintf(fplog,"#unmark_vertex xpix=%d ypix=%d\n",sx,sy);
    break;
  case RIGHTMOUSE:
    if (val == 0)  break;
    sx = getvaluator(MOUSEX);
    sy = getvaluator(MOUSEY);
    if (ctrlkeypressed) {
      getorigin(&ox,&oy);
      getsize(&lx,&ly);
      wx = sf*(sx-ox-lx/2.0)*2.0*fov/lx;
      wy = sf*(sy-oy-ly/2.0)*2.0*fov/ly;
      translate_brain(-wx,-wy,0.0);
      fprintf(fplog,"translate_brain_x %f\n",wx);
      fprintf(fplog,"translate_brain_y %f\n",wy);
      scale_brain(1.0/SCALE_UP_MOUSE);
      fprintf(fplog,"scale_brain %2.3f\n",1.0/SCALE_UP_MOUSE);
      redraw();
    }
    else {
      clear_all_vertex_marks();
      fprintf(fplog,"#clear_vertex_marks\n");
    }
    break;
  case REDRAW:         /* make the window re-sizable */
    reshapeviewport();
    last_frame_xdim = frame_xdim;
    getsize(&frame_xdim,&frame_ydim);
    /*if(last_frame_xdim != frame_xdim) redraw();*//* only if resize */
    fprintf(fplog,"resize_window %d\n",frame_xdim);break;
    break;
  case UPARROWKEY:
    if (val == 0)  break;
    rotate_brain(18.0,'x');
    fprintf(fplog,"rotate_brain_x 18\n");break;
  case DOWNARROWKEY:
    if (val == 0)  break;
    rotate_brain(-18.0,'x');
    fprintf(fplog,"rotate_brain_x -18\n");break;
  case LEFTARROWKEY:
    if (val == 0)  break;
    rotate_brain(18.0,'y');
    fprintf(fplog,"rotate_brain_y 18\n");break;
  case RIGHTARROWKEY:
    if (val == 0)  break;
    rotate_brain(-18.0,'y');
    fprintf(fplog,"rotate_brain_y -18\n");break;
  case PAD2:
    if (val == 0)  break;
    translate_brain(0.0,-10.0,0.0);
    fprintf(fplog,"translate_brain_y %f\n",-10.0);break;
  case PAD8:
    if (val == 0)  break;
    translate_brain(0.0,10.0,0.0);
    fprintf(fplog,"translate_brain_y %f\n",10.0);break;
  case PAD4:
    if (val == 0)  break;
    translate_brain(-10.0,0.0,0.0);
    fprintf(fplog,"translate_brain_x %f\n",-10.0);break;
  case PAD6:
    if (val == 0)  break;
    translate_brain(10.0,0.0,0.0);
    fprintf(fplog,"translate_brain_x %f\n",10.0);break;
  case DELKEY:
    if (val == 0)  break;
    rotate_brain(18.0,'z');
    fprintf(fplog,"rotate_brain_z 18\n");break;
  case PAGEDOWNKEY:
    if (val == 0)  break;
    rotate_brain(-18.0,'z');
    fprintf(fplog,"rotate_brain_z -18\n");break;
  case LEFTCTRLKEY:
    if (val)  ctrlkeypressed = TRUE;
    else      ctrlkeypressed = FALSE;
    break;
  case KEYBD:    /* marty: free: x y z H O Z @ # & * , . < > 3-9 - */
    switch((char)val)
            {
      case 'a': ncthreshflag = !ncthreshflag;
        fprintf(fplog,"set ncthreshflag %d\n",ncthreshflag);
        printf("surfer: ncthreshflag = %d\n",ncthreshflag);
        break;
      case 'A': areaflag = !areaflag;
        fprintf(fplog,"set areaflag %d\n",areaflag);
        printf("surfer: areaflag = %d\n",areaflag);break;
      case 'b': read_binary_patch(pfname);
        flag2d=TRUE;
        fprintf(fplog,"read_binary_patch\n");break;
      case 'B': write_binary_patch(pfname);
        fprintf(fplog,"write_binary_patch\n");break;
      case 'c': compute_curvature();
        fprintf(fplog,"compute_curvature\n");break;
      case 'C': clear_curvature();
        fprintf(fplog,"clear_curvature\n");break;
      case 'd': shrink(100);
        fprintf(fplog,"shrink 100\n");break;
      case 'D': shrink(1000);
        fprintf(fplog,"shrink 1000\n");break;
      case 'e': expandflag = !expandflag;
        fprintf(fplog,"set expandflag %d\n",expandflag);
        printf("surfer: expandflag = %d\n",expandflag);break;
      case 'E': printf("surfer: enter (pname stem ext) : ");
        scanf("%s %s %s",pname2,stem2,ext2);
        sprintf(tf2name,"%s/%s/%s%s.%s/COR-",
          subjectsdir,pname2,FILLDIR_STEM,
          stem2,ext2);
        read_image_info(tf2name);
        read_images(tf2name);
        shrinkfillflag = TRUE;
        MRIflag = TRUE;
        normalize_surface();
        fprintf(fplog,"#enter second surface\n");
        break;
      case 'f': find_orig_vertex_coordinates(selection);
        fprintf(fplog,"#find_orig_vertex_coordinates\n");break;
      case 'g': read_binary_areas(afname);
        fprintf(fplog,"read_binary_areas\n");break;
      case 'G': write_binary_areas(afname);
        fprintf(fplog,"write_binary_areas\n");break;
      case 'h': print_help_surfer();
        fprintf(fplog,"#print_help\n");break;
        break;
      case 'H': if (selection>=0)
        draw_cursor(selection,FALSE);
      printf("enter vertex index (%d): ",selection);
      scanf("%d",&selection);
      if (selection>=0)
        draw_cursor(selection,TRUE);
      PR
        break;
      case 'i': floodfill_marked_patch(RIPFILL);
        fprintf(fplog,"#floodfill_marked_patch\n");break;
      case 'I': floodfill_marked_patch(STATFILL);
        /*
    fill_surface();
    write_images(fill,fifname);
    fprintf(fplog,"#fill_surface\n");
    fprintf(fplog,"#write_images\n");
        */
        break;
      case 'j': cut_line(FALSE);
        fprintf(fplog,"#cut_line\n");break;
      case 'J': cut_plane();
        fprintf(fplog,"#cut_plane\n");break;
      case 'k': avgflag = !avgflag;
        fprintf(fplog,"set avgflag %d\n",avgflag);
        printf("surfer: set avgflag = %d\n",avgflag);break;
      case 'K': read_binary_curvature(kfname);
        fprintf(fplog,"read_binary_sulc\n");break;
      case 'l': smooth_curv(5);
        fprintf(fplog,"smooth_curv 5\n");break;
      case 'L': flatten(tfname);flag2d=TRUE;
        fprintf(fplog,"#flatten\n");break;
      case 'm': smooth_val_sparse(5);
        fprintf(fplog,"smooth_val_sparse 5\n");break;
      case 'M': MRIflag = !MRIflag;
        fprintf(fplog,"set MRIflag %d\n",MRIflag);
        printf("surfer: set MRIflag %d\n",MRIflag);break;
        /*case 'n': subsample_orient(0.03);break; */
      case 'n': 
        printf("enter dipole spacing (in mm): ");
        scanf("%lf",&dip_spacing);
        subsample_dist((int)dip_spacing);
        break;
      case 'N': 
        write_binary_dipoles(dfname);
        fprintf(fplog,"write_binary_dipoles\n");
        sprintf(sfname,"%s/%s/%s/%s-%d.dec",
          subjectsdir,pname,BEM_DIR,
          stem,(int)dip_spacing);
        write_binary_decimation(sfname);
        fprintf(fplog,"write_binary_decimation\n");
        break;
      case 'o': overlayflag = !overlayflag;
        fprintf(fplog,"set overlayflag %d\n",overlayflag);
        printf("surfer: set overlayflag = %d\n",overlayflag);
        break;
      case 'O': /*** free ***/;
        fprintf(fplog,"\n");
        printf("surfer:\n");break;
      case 'p': cut_vertex();
        break;
      case 'P': cut_line(TRUE);break;
      case 'q': read_binary_values(vfname);break;
      case 'Q': write_binary_values(vfname);break;
      case 'r': redraw();fflush(fplog);
        fprintf(fplog,"redraw\n");break;
      case 'R': read_binary_curvature(cfname);
        fprintf(fplog,"read_binary_curv\n");break;
      case 's': shrink(1);
        fprintf(fplog,"shrink 1\n");break;
      case 'S': shrink(10);
        fprintf(fplog,"shrink 10\n");break;
      case 't': prompt_for_drawmask();
        fprintf(fplog,"set surfcolor %d\n",surfcolor);
        fprintf(fplog,"set shearvecflag %d\n",shearvecflag);
        fprintf(fplog,"set normvecflag %d\n",normvecflag);
        fprintf(fplog,"set movevecflag %d\n",movevecflag);
        redraw();break;
      case 'T': save_rgb_num(gfname);
        fprintf(fplog,"save_rgb_num\n");break;
      case 'u': prompt_for_parameters();
        fprintf(fplog,"set wt %2.3f\n",wt);
        fprintf(fplog,"set wa %2.3f\n",wa);
        fprintf(fplog,"set ws %2.3f\n",ws);
        fprintf(fplog,"set wn %2.3f\n",wn);
        fprintf(fplog,"set wc %2.3f\n",wc);
        fprintf(fplog,"set wsh %2.3f\n",wsh);
        fprintf(fplog,"set wbn %2.3f\n",wbn);
        break;
      case 'U': normalize_area();
        fprintf(fplog,"normalize_area\n");break;
      case 'v': verticesflag = !verticesflag;
        fprintf(fplog,"set verticesflag %d\n",verticesflag);
        printf("surfer: set verticesflag = %d\n",verticesflag);
        break;
      case 'V': if (surfcolor==NONE)
        surfcolor = CURVATURE_OR_SULCUS;
      else if (surfcolor==CURVATURE_OR_SULCUS)
        surfcolor = NONE;
      else ;
      fprintf(fplog,"set surfcolor %d\n",surfcolor);
      printf("surfer: set surfcolor = %d\n",surfcolor);break;
      case 'w': write_binary_surface(ofname);break;
      case 'W': write_binary_curvature(cfname);break;
      case 'x': /*** free ***/;
        printf("\n");
        fprintf(fplog,"\n");break;
      case 'X': draw_spokes(RADIUS);
        fprintf(fplog,"draw_radius\n");break;
      case 'y': /*** free ***/;
        printf("\n");
        fprintf(fplog,"\n");break;
      case 'Y': draw_spokes(THETA);
        fprintf(fplog,"draw_theta\n");break;
      case 'z': /*** free ***/;
        printf("\n");
        fprintf(fplog,"\n");break;
      case 'Z': /*** free ***/;
        printf("\n");
        fprintf(fplog,"\n");break;
      case '*': fslope *= 1.5; cslope *= 1.5;
        printf("surfer: fslope = %f, cslope = %f\n",
         fslope,cslope);
        fprintf(fplog,"set fslope %2.2f\n",fslope);
        fprintf(fplog,"set cslope %2.2f\n",cslope);
        break;
      case '/': fslope /= 1.5; cslope /= 1.5;
        printf("surfer: fslope = %f, cslope = %f\n",
         fslope,cslope);
        fprintf(fplog,"set fslope %2.2f\n",fslope);
        fprintf(fplog,"set cslope %2.2f\n",cslope);
        break;
      case '!': fslope = cslope = 1.0; mslope = 0.05; break;
      case '+': cmid *= 1.1;
        mmid *= 1.1;
        fprintf(fplog,"#sigmoid mid up\n");
        break;
      case '-': cmid /= 1.1;
        mmid /= 1.1;
        fprintf(fplog,"#sigmoid mid down\n");
        break;
      case '0': restore_zero_position();
        fprintf(fplog,"restore_zero_position\n");break;
      case '1': restore_initial_position();
        fprintf(fplog,"restore_initial_position\n");break;
      case '2': make_lateral_view(stem);
        fprintf(fplog,"make_lateral_view\n");break;
      case '(': cthk -= 0.1;
        printf("surfer: cthk = %f\n",cthk);
        fprintf(fplog,"set cthk %1.1f\n",cthk);break;
      case ')': cthk += 0.1;
        printf("surfer: cthk = %f\n",cthk);
        fprintf(fplog,"set cthk %1.1f\n",cthk);break;
      case '{': scale_brain(1/SCALE_UP_LG);
        fprintf(fplog,"scaledown_lg\n");break;
      case '}': scale_brain(SCALE_UP_LG);
        fprintf(fplog,"scaleup_lg\n");break;
      case '[': scale_brain(1/SCALE_UP_SM);
        fprintf(fplog,"scaledown_sm\n");break;
      case ']': scale_brain(SCALE_UP_SM);
        fprintf(fplog,"scaleup_sm\n");break;
      case '?': print_help_surfer();
        fprintf(fplog,"#print_help\n");break;
            }
    break;
  }  /* dev switch */
      }    /* was qtest (omitted) */
      if (autoflag) redraw();
      
    }  /* while events */
#endif
#endif  /* tcl: omit event loop */
  if (patch_name)
    {
      strcpy(pfname, patch_name) ;
      read_binary_patch(patch_name) ;
      restore_zero_position() ;
      rotate_brain(-90.0, 'x') ;
      redraw() ;
    }
  
  return(0) ;
}

#ifdef TCL
int
do_one_gl_event(Tcl_Interp *interp)   /* tcl */
{
  
#ifdef OPENGL     /* derived from event.c:DoNextEvent(),tkExec() */
  XEvent current, ahead;
  char buf[1000],*tclvar;
  char command[NAME_LENGTH];
  KeySym ks;
  static int ctrlkeypressed = FALSE;
  static int altkeypressed = FALSE;
  static int shiftkeypressed = FALSE;
  static int button1pressed = FALSE;
  static int button2pressed = FALSE;
  static int button3pressed = FALSE;
  short sx,sy;
  long ox,oy,lx,ly;
  float wx, wy;
  XWindowAttributes wat;
  Window junkwin;
  int rx, ry;
  /* begin rkt */
  int mvno;
  float md;
  /* end rkt */
  
  if (!openglwindowflag) return(0);
  
  if (XPending(xDisplay)) {  /* do one if queue test */
    
    XNextEvent(xDisplay, &current);   /* blocks here if no event */
    
    switch (current.type) {
      
    case ConfigureNotify:
      XGetWindowAttributes(xDisplay, w.wMain, &wat);
      XTranslateCoordinates(xDisplay, w.wMain, wat.root,
          -wat.border_width, -wat.border_width,
          &rx, &ry, &junkwin);
      w.x = rx;  /* left orig         (current.xconfigure.x = 0 relative!) */
      w.y = ry;  /* top orig: +y down (current.xconfigure.y = 0 relative!) */
      w.w = current.xconfigure.width;
      w.h = current.xconfigure.height;
      resize_window(0);
      tclvar = Tcl_GetVar(interp,"tksurferinterface",TCL_GLOBAL_ONLY);
      /* begin rkt */
#if 0
      if (followglwinflag && tclvar!=NULL &&
    (MATCH(tclvar,"micro") || MATCH(tclvar,"mini"))) {
  sprintf(command,"wm geometry .w +%d+%d",
    w.x, w.y + w.h + MOTIF_YFUDGE /*+MOTIF_XFUDGE*/);
  Tcl_Eval(interp,command);
  /*Tcl_Eval(interp,"raise .");*/
      }        
#else
      /* move the tool window under us */
      sprintf(command,"MoveToolWindow %d %d",
        w.x, w.y + w.h + MOTIF_YFUDGE /*+MOTIF_XFUDGE*/);
      Tcl_Eval(interp,command);
      
      /* update window position */
      curwindowleft = w.x;
      curwindowbottom = w.y + w.h;
#endif

      /* if our redraw lock flag is on, redraw the window. check to
   see if there are any expose or configure events ahead of us,
   and if so, don't redraw now. this saves us from having
   multiple redraw flashes. */
      if (redrawlockflag) 
  {
    if (XPending(xDisplay)) 
      {
        XPeekEvent(xDisplay, &ahead);
        if (ahead.type==Expose || ahead.type==ConfigureNotify) 
    break;
      }
    Tcl_Eval(interp,"UpdateAndRedraw");
  }
      /* end rkt */
      break;
      
    case Expose:
      /* begin rkt */
      /* if our redraw lock flag is on, redraw the window. check to
   see if there are any expose or configure events ahead of us,
   and if so, don't redraw now. this saves us from having
   multiple redraw flashes. */
      if (redrawlockflag) 
  {
    if (XPending(xDisplay)) 
      {
        XPeekEvent(xDisplay, &ahead);
        if (ahead.type==Expose || ahead.type==ConfigureNotify) 
    break;
      }
    Tcl_Eval(interp,"UpdateAndRedraw");
  }
#if 0
      if (XPending(xDisplay)) {
  XPeekEvent(xDisplay, &ahead);
  if (ahead.type==Expose || ahead.type==ConfigureNotify) break;
      }
      /*Tcl_Eval(interp,"redrawbutton");*/ /* still multiple */
#endif
      /* end rkt */
      break;
      
      /* begin rkt */
    case MotionNotify:
      if (mouseoverflag) 
  {
    if (XPending(xDisplay)) {
      XPeekEvent(xDisplay, &ahead);
      if (ahead.type==MotionNotify)
        break;
    }
    sx = current.xmotion.x;
    sy = current.xmotion.y;
    sx += w.x;   /* convert back to screen pos (ugh) */
    sy = 1024 - w.y - sy;
    find_vertex_at_screen_point( sx, sy, &mvno, &md );
    if( mvno >= 0 )
      update_labels(LABELSET_MOUSEOVER, mvno, md);
  }
      break;
      /* end rkt */
      
    case ButtonPress:
      sx = current.xbutton.x;
      sy = current.xbutton.y;
      sx += w.x;   /* convert back to screen pos (ugh) */
      sy = 1024 - w.y - sy;
      if (current.xbutton.button == 1) {  /** left **/
  button1pressed = TRUE;
  if (ctrlkeypressed) { /* scale around click */
    getorigin(&ox,&oy);
    getsize(&lx,&ly);
    wx = sf*(sx-ox-lx/2.0)*2.0*fov/lx;
    wy = sf*(sy-oy-ly/2.0)*2.0*fov/ly;
    /* begin rkt */
#if 0
    sprintf(command,"set xtrans %d; set ytrans %d",(int)-wx,(int)-wy);
    Tcl_Eval(interp,command);
    sprintf(command,"set scalepercent %d",(int)SCALE_UP_MOUSE*100);
    Tcl_Eval(interp,command);
    Tcl_Eval(interp,"redrawbutton");
#endif
    translate_brain (-wx, -wy, 0);
    scale_brain (SCALE_UP_MOUSE);
    redraw();
    /* end rkt */
  }
  else if (shiftkeypressed) {  /* curvim */
    select_vertex(sx,sy);
    read_curvim_at_vertex(selection);
    draw_cursor(selection,TRUE);
  }
  else {
    
    /* begin rkt */
    if (selection>=0)
      {
        draw_vertex_hilite(selection);
        draw_cursor(selection,FALSE);
      }
    select_vertex(sx,sy);
    if (selection>=0)
      {
        mark_vertex(selection,TRUE);
        draw_cursor(selection,TRUE);
      }
    /* draw all the marked verts. */
    draw_marked_vertices ();
    /* end rkt */
    if (showorigcoordsflag)
      find_orig_vertex_coordinates(selection);
  }
      }
      if (current.xbutton.button == 2) {  /** middle **/
  button2pressed = TRUE;
  select_vertex(sx,sy);
  if (selection>=0)
    mark_vertex(selection,FALSE);
      }
      if (current.xbutton.button == 3) {  /** right **/
  button3pressed = TRUE;
  if (ctrlkeypressed) {
    getorigin(&ox,&oy);
    getsize(&lx,&ly);
    wx = sf*(sx-ox-lx/2.0)*2.0*fov/lx;
    wy = sf*(sy-oy-ly/2.0)*2.0*fov/ly;
    /* begin rkt */
#if 0
    sprintf(command,"set xtrans %d; set ytrans %d",(int)-wx,(int)-wy);
    Tcl_Eval(interp,command);
    sprintf(command,"set scalepercent %d",(int)(1.0/SCALE_UP_MOUSE*100));
    Tcl_Eval(interp,command);
    Tcl_Eval(interp,"redrawbutton");
#endif
    translate_brain (-wx, -wy, 0);
    scale_brain (1.0/SCALE_UP_MOUSE);
    redraw();
    /* end rkt */
  }
  else {
    clear_all_vertex_marks();
    /* begin rkt */
    redraw();
    /* end rkt */
  }
      }
      break;
      
    case ButtonRelease:
      if (current.xbutton.button == 1)  button1pressed = FALSE;
      if (current.xbutton.button == 2)  button2pressed = FALSE;
      if (current.xbutton.button == 3)  button3pressed = FALSE;
      break;
      
      
    case KeyPress:
      XLookupString(&current.xkey, buf, sizeof(buf), &ks, 0);
      switch (ks) {
  
  /* numbers */
      case XK_0: if (altkeypressed)  ; break;
  
  /* upper case */
      case XK_A: if (altkeypressed)  ; break;
  
  /* lower case */
  /* begin rkt */
#if 0
      case XK_r: if (altkeypressed) Tcl_Eval(interp,"redrawbutton"); break;
#endif
      case XK_r: 
  if (altkeypressed) 
    Tcl_Eval(interp,"UpdateAndRedraw"); 
  break;
  /* end rkt */
  
  /* others */
  /* begin rkt */
#if 0
      case XK_Up:
  if (altkeypressed) Tcl_Eval(interp,"set xrot [expr $xrot+18.0]");
  break;
      case XK_Down:
  if (altkeypressed) Tcl_Eval(interp,"set xrot [expr $xrot-18.0]");
  break;
      case XK_Right:
  if (altkeypressed) Tcl_Eval(interp,"set yrot [expr $yrot-18.0]");
  break;
      case XK_Left:
  if (altkeypressed) Tcl_Eval(interp,"set yrot [expr $yrot+18.0]");
  break;
#endif
      case XK_Up:
  if (altkeypressed) 
    Tcl_Eval(interp,"set gNextTransform(rotate,x) "
       "[expr $gNextTransform(rotate,x)+18.0]");
  break;
      case XK_Down:
  if (altkeypressed) 
    Tcl_Eval(interp,"set gNextTransform(rotate,x) "
       "[expr $gNextTransform(rotate,x)-18.0]");
  break;
      case XK_Right:
  if (altkeypressed) 
    Tcl_Eval(interp,"set gNextTransform(rotate,y) "
       "[expr $gNextTransform(rotate,y)-18.0]");
  break;
      case XK_Left:
  if (altkeypressed) 
    Tcl_Eval(interp,"set gNextTransform(rotate,y) "
       "[expr $gNextTransform(rotate,y)+18.0]");
  break;
      case XK_Home:
  sprintf(command,"MoveToolWindow %d %d",
    curwindowleft, curwindowbottom + MOTIF_YFUDGE);
  Tcl_Eval(interp,command);
  break;
  /* end rkt */
  
  /* modifiers */
      case XK_Shift_L:   case XK_Shift_R:   shiftkeypressed=TRUE;  break;
      case XK_Control_L: case XK_Control_R: ctrlkeypressed=TRUE;   break;
      case XK_Alt_L:     case XK_Alt_R:     altkeypressed=TRUE;    break;
      }
      break;
      
    case KeyRelease:   /* added this mask to owindow.c */
      XLookupString(&current.xkey, buf, sizeof(buf), &ks, 0);
      switch (ks) {
      case XK_Shift_L:   case XK_Shift_R:   shiftkeypressed=FALSE; break;
      case XK_Control_L: case XK_Control_R: ctrlkeypressed=FALSE;  break;
      case XK_Alt_L:     case XK_Alt_R:     altkeypressed=FALSE;   break;
      }
      break;
      
    }
    return GL_FALSE;
  }
  
#else  /* use gl calls */
  short dev, val;
  static int ctrlkeypressed = FALSE;
  static int altkeypressed = FALSE;
  static int shiftkeypressed = FALSE;
  short sx,sy; /* Screencoord sx,sy; */
  float wx,wy; /* Coord wx,wy; */
  long ox,oy,lx,ly;
  char fname[200],fpref[200];
  int i;
  FILE *fp;
  float a,b,c;
  
  float tmid,dt,f1,f2,f3;
  int num;
  
  blinkbuffers();
  
  if (qtest()) {  /* do one event */
    dev = qread(&val);
    if (dev != LEFTMOUSE && dev != RIGHTMOUSE) /* hack: mouse zeros getbutton!*/
      ctrlkeypressed = getbutton(LEFTCTRLKEY) || getbutton(RIGHTCTRLKEY);
    switch(dev) {
    case REDRAW:
      resize_window(0);   /* reshape flag */
      break;
    case LEFTMOUSE:
      if (val == 0)  break;
      sx = getvaluator(MOUSEX);
      sy = getvaluator(MOUSEY);
      if (ctrlkeypressed) { /* scale around click */
  getorigin(&ox,&oy);
  getsize(&lx,&ly);
  wx = sf*(sx-ox-lx/2.0)*2.0*fov/lx;
  wy = sf*(sy-oy-ly/2.0)*2.0*fov/ly;
  translate_brain(-wx,-wy,0.0);
  scale_brain(SCALE_UP_MOUSE);
  redraw();
      }
      else if (shiftkeypressed) {
  select_vertex(sx,sy);
  read_curvim_at_vertex(selection);
  draw_cursor(selection,TRUE);
      }
      else {
  if (selection>=0)
    draw_cursor(selection,FALSE);
  select_vertex(sx,sy);
  if (selection>=0)
    mark_vertex(selection,TRUE);
  if (selection>=0)
    draw_cursor(selection,TRUE);
      }
      break;
    case MIDDLEMOUSE:
      if (val == 0)  break;
      sx = getvaluator(MOUSEX);
      sy = getvaluator(MOUSEY);
      select_vertex(sx,sy);
      if (selection>=0)
  mark_vertex(selection,FALSE);
      break;
    case RIGHTMOUSE:
      if (val == 0)  break;
      sx = getvaluator(MOUSEX);
      sy = getvaluator(MOUSEY);
      if (ctrlkeypressed) {
  getorigin(&ox,&oy);
  getsize(&lx,&ly);
  wx = sf*(sx-ox-lx/2.0)*2.0*fov/lx;
  wy = sf*(sy-oy-ly/2.0)*2.0*fov/ly;
  translate_brain(-wx,-wy,0.0);
  scale_brain(1.0/SCALE_UP_MOUSE);
  redraw();
      }
      else {
  clear_vertex_marks();
      }
      break;
    case UPARROWKEY:
      if (!altkeypressed) break;
      if (val == 0)  break;
      rotate_brain(18.0,'x'); break;
    case DOWNARROWKEY:
      if (!altkeypressed) break;
      if (val == 0)  break;
      rotate_brain(-18.0,'x'); break;
    case LEFTARROWKEY:
      if (!altkeypressed) break;
      if (val == 0)  break;
      rotate_brain(18.0,'y'); break;
    case RIGHTARROWKEY:
      if (!altkeypressed) break;
      if (val == 0)  break;
      rotate_brain(-18.0,'y'); break;
    case PAD2:
      if (!altkeypressed) break;
      if (val == 0)  break;
      translate_brain(0.0,-10.0,0.0); break;
    case PAD8:
      if (!altkeypressed) break;
      if (val == 0)  break;
      translate_brain(0.0,10.0,0.0); break;
    case PAD4:
      if (!altkeypressed) break;
      if (val == 0)  break;
      translate_brain(-10.0,0.0,0.0); break;
    case PAD6:
      if (!altkeypressed) break;
      if (val == 0)  break;
      translate_brain(10.0,0.0,0.0); break;
    case DELKEY:
      if (!altkeypressed) break;
      if (val == 0)  break;
      rotate_brain(18.0,'z'); break;
    case PAGEDOWNKEY:
      if (!altkeypressed) break;
      if (val == 0)  break;
      rotate_brain(-18.0,'z'); break;
    case LEFTALTKEY: case RIGHTALTKEY:
      if (val)  altkeypressed = TRUE;
      else      altkeypressed = FALSE;
      break;
    case LEFTSHIFTKEY: case RIGHTSHIFTKEY:
      if (val)  shiftkeypressed = TRUE;
      else      shiftkeypressed = FALSE;
      break;
    case KEYBD:
      if (altkeypressed)
        switch((char)val)
    {
          case 'f': find_orig_vertex_coordinates(selection);
      break;
          case 'F': select_orig_vertex_coordinates(&selection);
      break;
          case 'o': read_orig_vertex_coordinates(orfname);
      break;
          case 'O': printf("enter a, b, c: ");
      scanf("%f %f %f",&a,&b,&c);
      read_ellipsoid_vertex_coordinates(elfname,a,b,c);
      break;
          case 'i': floodfill_marked_patch(RIPFILL);
      printf("floodfill_marked_patch(%d)\n",RIPFILL);
      break;
          case 'I': floodfill_marked_patch(STATFILL);
      printf("floodfill_marked_patch(%d)\n",STATFILL;
       break;
       case 'u': restore_ripflags(1);
       printf("restore_ripflags(1)\n");
       break;
       case 'j': cut_line(FALSE);
       printf("cut_line\n");
       break;
       case 'J': cut_plane();
       printf("cut_plane\n");
       break;
       case 'L': flatten(tfname);
       flag2d=TRUE;
       printf("flatten\n");
       break;
       case 'p': cut_vertex();
       break;
       case 'P': cut_line(TRUE);
       break;
       case 'r': redraw();PR
       break;
       case 'R': printf("enter .iop-name and dipole-file-number: ");
       scanf("%s %d",fname,&num);
       read_iop(fname,num);
       printf("enter number of rec files: ");
       scanf("%d",&num);
       sol_nrec = 0;
       for (i=0;i<num;i++)
      {
        printf("enter .rec-name: ");
        scanf("%s",fname);
        read_rec(fname);
        filter_rec(sol_nrec-1);
      }
       compute_timecourses();
       PR
       break;
       case 'T': if (selection>=0)
       draw_cursor(selection,FALSE);
       find_nearest_fixed(selection,&selection);
       if (selection>=0)
       draw_cursor(selection,TRUE);
       PR
       break;
       case 't': plot_nearest_nonzero(selection);
       break;
       case 'l': printf("enter .vlist file name: ");
       scanf("%s",fname);
       read_plot_list(fname);
       PR
       break;
       /*
         case 'I': printf("enter .pri file name: ");
         scanf("%s",fname);
         read_binary_values(fname);
         PR
         break;
       */
       case 'D': printf("enter .dec file name: ");
       scanf("%s",fpref);
       sprintf(fname,"%s",fpref);
       read_binary_decimation(fname);
       printf("enter .dip file name: ");
       scanf("%s",fpref);
       sprintf(fname,"%s",fpref);
       read_binary_dipoles(fname);
       PR
       break;
       case 'd': printf("enter tmid dt num: ");
       scanf("%f %f %d",&tmid,&dt,&num);
       load_vals_from_sol(tmid,dt,num);
       break;
       case 'C': if (sol_nrec>1)
      {
        printf("enter num: ");
        scanf("%d",&num);
      }
       else
       num = 0;
       load_var_from_sol(num);
       PR
       break;
       case 'c': printf("enter plot type: ");
       scanf("%d",&sol_plot_type);
       PR
       break;
       case 'S': printf("enter type: ");
       scanf("%d",&num);
       normalize_time_courses(num);
       break;
       case 's': printf("enter pthresh (%f) pslope (%f) maxrat (%f): ",
            sol_pthresh,sol_pslope,sol_maxrat);
       scanf("%f %f %f",&f1,&f2,&f3);
       sol_pthresh = f1;
       sol_pslope = f2;
       sol_maxrat = f3;
       PR
       break;
       case 'x': compute_select_crosstalk();
       PR
       break;
       case 'X': printf("enter type (0=unweighted, 1=distance-weighted): ");
       scanf("%d",&num);
       compute_all_crosstalk(num);
       PR
       break;
       case 'g': compute_select_pointspread();
       PR
       break;
       case 'G': compute_all_pointspread();
       PR
       break;
       case 'H': if (selection>=0)
       draw_cursor(selection,FALSE);
       printf("enter vertex index (%d): ",selection);
       scanf("%d",&selection);
       if (selection>=0)
       draw_cursor(selection,TRUE);
       PR
       break;
       case 'h': printf("enter min max nbins: ");
       scanf("%f %f %d",&f1,&f2,&num);
       write_val_histogram(f1,f2,num);
       PR
       break;
       }
    }
    }}
#endif
    return(0) ;
  }
#endif
  
  
void
read_ncov(char *fname)
{
  int i,j,tnmeg,tneeg;
  float f;
  FILE *fptr;
  float regconst=0.01;
  
  printf("read_ncov(%s)\n",fname);
  fptr = fopen(fname,"r");
  if (fptr==NULL) {printf("can't find file %s\n",fname);exit(1);}
  fscanf(fptr,"%*s");
  fscanf(fptr,"%d %d",&tnmeg,&tneeg);
  if ((sol_nmeg!=0 && tnmeg!=sol_nmeg) || (sol_nmeg!=0 && tnmeg!=sol_nmeg))
    {
      printf("incompatible nmeg or meeg in ncov file\n");
      exit(1);
    }
  sol_NoiseCovariance = MGH_matrix(sol_nchan,sol_nchan);
  for (i=0;i<tneeg+tnmeg;i++)
    for (j=0;j<tneeg+tnmeg;j++)
      {
  fscanf(fptr,"%f",&f);
  if (sol_nmeg==0)
    {
      if (i<tneeg && j<tneeg)
        sol_NoiseCovariance[i][j] = f;
    } else
      if (sol_neeg==0)
        {
    if (i>=tneeg && j>=tneeg)
      sol_NoiseCovariance[i-tneeg][j-tneeg] = f;
        } else
    sol_NoiseCovariance[i][j] = f;
      }
  /*
    for (i=0;i<sol_nchan;i++)
    printf("%d: %9.4e\n",i,sol_NoiseCovariance[i][i]);
  */
  regularize_matrix(sol_NoiseCovariance,sol_nchan,regconst);
  fclose(fptr);
}

void
regularize_matrix(FLOATTYPE **m, int n, float fact)
{
  int i;                   /* loop counters */
  float sum;
  
  sum = 0;
  for (i=0;i<n;i++)
    sum += m[i][i];
  sum /= n;
  /*
    printf("avg. diag. = %g\n",sum);
  */
  if (sum==0) sum=1;
  if (sum!=0)
    for (i=0;i<n;i++)
      m[i][i] += sum*fact;
}

void
read_iop(char *fname, int dipfilenum)
{
  int i,j,k,jc,d;
  FILE *fp;
  char c,str[200];
  float f;
  int iop_vernum = 0;
  
  printf("read_iop(%s,%d)\n",fname,dipfilenum);
  
  sol_pthresh = 1000;
  fp = fopen(fname,"r");
  if (fp==NULL) {printf("surfer: ### can't find file %s\n",fname);PR return;}
  c = fgetc(fp);
  if (c=='#')
    {
      fscanf(fp,"%s",str);
      if (MATCH_STR("version"))
  fscanf(fp,"%d",&iop_vernum);
      printf("iop version = %d\n",iop_vernum);
      fscanf(fp,"%d %d %d %d",&sol_neeg,&sol_nmeg,&sol_nperdip,&sol_ndipfiles);
      sol_nchan = sol_neeg+sol_nmeg;
      for (i=1;i<=dipfilenum;i++)
  {
    fscanf(fp,"%d %d",&sol_ndip,&sol_nnz);
    if (i==dipfilenum)
      {
        if (sol_ndip != sol_ndec)
    {
      printf(".dec and .iop file mismatch (%d!=%d)\n",
       sol_ndip,sol_ndec);
      fclose(fp);
      return;
    }
        sol_W = MGH_matrix(sol_nnz*sol_nperdip,sol_nchan);
        if (iop_vernum==1)
    sol_A = MGH_matrix(sol_nchan,sol_nnz*sol_nperdip);
        
        sol_M = MGH_matrix(sol_nchan,sol_nchan); /* temp. space for xtalk */
        sol_Mi = MGH_matrix(sol_nchan,sol_nchan);
        sol_sensvec1 = MGH_vector(sol_nchan);
        sol_sensvec2 = MGH_vector(sol_nchan);
        
        sol_sensval = MGH_vector(sol_nchan);
        sol_dipindex = MGH_ivector(sol_nnz);
        sol_pval = MGH_vector(sol_nnz);
        sol_prior = MGH_vector(sol_nnz);
        sol_dipfact = MGH_vector(sol_nnz);
      }
    for (j=0;j<sol_nnz;j++)
      {
        if (i==dipfilenum)
    {
      fscanf(fp,"%d",&d);
      sol_dipindex[j] = d;
    }
        else
    fscanf(fp,"%*d");
      }
    for (j=0;j<sol_nnz;j++)
      {
        if (i==dipfilenum)
    {
      fscanf(fp,"%f",&f);
      sol_pval[j] = f;
      f = fabs(f);
      if (f<sol_pthresh) sol_pthresh = f;
    }
        else
    fscanf(fp,"%*f");
      }
    for (j=0;j<sol_nnz;j++)
      {
        if (i==dipfilenum)
    {
      fscanf(fp,"%f",&f);
      sol_prior[j] = f;
      mris->vertices[sol_dipindex[j]].val = f;
    }
        else
    fscanf(fp,"%*f");
      }
    for (j=0;j<sol_nnz;j++)
      for (jc=0;jc<sol_nperdip;jc++)
        for (k=0;k<sol_nchan;k++)
    {
      if (i==dipfilenum)
        {
          fscanf(fp,"%f",&f);
          sol_W[j*sol_nperdip+jc][k] = f;
        }
      else
        fscanf(fp,"%*f");
    }
    if (iop_vernum==1)
      for (j=0;j<sol_nnz;j++)
        for (jc=0;jc<sol_nperdip;jc++)
    for (k=0;k<sol_nchan;k++)
      {
        if (i==dipfilenum)
          {
      fscanf(fp,"%f",&f);
      sol_A[k][j*sol_nperdip+jc] = f;
          }
        else
          fscanf(fp,"%*f");
      }
  }
    }
  else
    {
      printf("Can't read binary .iop files\n");
    }
  fclose(fp);
}

void
read_rec(char *fname)
{
  int i,j,tntime,tptime,tnmeg,tneeg,tnchan;
  float f;
  FILE *fp;
  
  printf("read_rec(%s)\n",fname);
  
  fp = fopen(fname,"r");
  if (fp==NULL) {printf("can't find file %s\n",fname);exit(1);}
  if (sol_nmeg+sol_nmeg<=0)
    {
      printf("iop-file must be read before rec-file\n");
      return;
    }
  fscanf(fp,"%*s");
  fscanf(fp,"%d %d %d",&tntime,&tnmeg,&tneeg);
  tnchan = tnmeg+tneeg;
  if (sol_ntime>0 && sol_ntime!=tntime)
    {
      printf("ntime does not match tntime (%d != %d)\n",sol_ntime,tntime);
      exit(1);
    }
  tptime = (int)(2*pow(2.0,ceil(log((float)tntime)/log(2.0))));
  printf("tntime=%d\n",tntime);
  printf("tptime=%d\n",tptime);
  sol_ntime = tntime;
  sol_ptime = tptime;
  if (sol_nmeg>0 && sol_nmeg!=tnmeg)
    {
      printf("nmeg does not match tnmeg (%d != %d)\n",sol_nmeg,tnmeg);
      exit(1);
    }
  if (sol_neeg>0 && sol_neeg!=tneeg)
    {
      printf("neeg does not match tnmeg (%d != %d)\n",sol_neeg,tneeg);
      exit(1);
    }
  if (sol_nrec==0)
    sol_lat = MGH_vector(sol_ptime);
  sol_Data[sol_nrec] = MGH_matrix(tnchan,sol_ptime);
  for (j=0;j<tntime;j++)
    {
      fscanf(fp,"%f",&f);
      sol_lat[j] = f;
      for (i=0;i<tneeg;i++)
  {
    fscanf(fp,"%f",&f);
    if (sol_neeg>0)
      sol_Data[sol_nrec][i][j] = f;
  }
      for (i=0;i<tnmeg;i++)
  {
    fscanf(fp,"%f",&f);
    if (sol_nmeg>0)
      sol_Data[sol_nrec][i+sol_neeg][j] = f;
  }
    }
  fclose(fp);
  sol_sample_period = sol_lat[1]-sol_lat[0];
  sol_epoch_begin_lat = sol_lat[0];
  sol_dipcmp_val[sol_nrec] = MGH_matrix(sol_nnz*sol_nperdip,sol_ntime);
  sol_nrec++;
}
void
filter_recs(void)
{
  int i;
  
  for (i=0;i<sol_nrec;i++)
    filter_rec(i);
}

void
filter_rec(int np)
{
  int i,j,k,n,i0,i1;
  float sum;
  
  if (sol_trendflag)
    for (i=0;i<sol_nchan;i++)
      remove_trend(sol_Data[np][i],sol_ntime);
  for (i=0;i<sol_nchan;i++)
    for (j=sol_ntime;j<sol_ptime;j++)
      sol_Data[np][i][j] =
  sol_Data[np][i][sol_ntime-1]*(1-(j-(sol_ntime-1))/((float)sol_ptime-(sol_ntime-1
                       )))+
  sol_Data[np][i][0]*(j-(sol_ntime-1))/((float)sol_ptime-(sol_ntime-1));
  
  bpfilter(sol_Data[np],sol_nchan,sol_ptime,sol_loflim*sol_ptime*sol_sample_period/1000.0,
     sol_hiflim*sol_ptime*sol_sample_period/1000.0);
  
  i1 = floor((sol_baseline_end-sol_epoch_begin_lat)/sol_sample_period+0.5);
  i0 = floor(i1-sol_baseline_period/sol_sample_period+0.5);
  if (i0<0) i0 = 0;
  for (k=0;k<sol_nchan;k++)
    {
      for (j=i0,n=0,sum=0;j<=i1;j++,n++)
  sum += sol_Data[np][k][j];
      sum /= n;
      for (j=0;j<sol_ptime;j++)
  sol_Data[np][k][j] -= sum;
    }
}

void
remove_trend(FLOATTYPE *v, int n)
{
  int k;
  double a,b,c1,c2,c11,c12,c21,c22,f;
  
  c1 = c2 = c11 = c12 = c21 = c22 = 0;
  for (k=0;k<n;k++)
    {
      f = v[k];
      c1 += f;
      c2 += k*f;
      c11 += k;
      c12 += 1;
      c21 += k*k;
      c22 += k;
    }
  a = (c1*c22-c2*c12)/(c11*c22-c12*c21);
  b = (c2*c11-c1*c21)/(c11*c22-c12*c21);
  for (k=0;k<n;k++)
    v[k] -= a*k+b;
  /*
    printf("remove trend: a=%f, b=%f\n",a,b);
  */
}

void
compute_timecourses(void)
{
  int i;
  
  for (i=0;i<sol_nrec;i++)
    compute_timecourse(i);
}

void
compute_timecourse(int num)
{
  int i,j,k,jc;
  double sum;
  
  printf("compute_timecourse(%d)\n",num);
  for (i=0;i<sol_ntime;i++)
    {
      for (j=0;j<sol_nnz;j++)
  for (jc=0;jc<sol_nperdip;jc++)
    {
      sum = 0;
      for (k=0;k<sol_nchan;k++)
        sum += sol_W[j*sol_nperdip+jc][k]*sol_Data[num][k][i];
      sol_dipcmp_val[num][j*sol_nperdip+jc][i] = sum;
    }
    }
}

void
plot_all_time_courses(void)
{
  int j,xpos,ypos;
  
  printf("plot_all_time_courses()\n");
  for (j=0;j<sol_nplotlist;j++)
    {
      xpos = PLOT_XFUDGE+(j%6)*(2*PLOT_XFUDGE+PLOT_XDIM);
      ypos = PLOT_YFUDGE+(j/6)*(PLOT_YFUDGE+PLOT_XFUDGE+PLOT_YDIM);
      plot_sol_time_course(sol_plotlist[j],PLOT_XDIM,PLOT_YDIM,xpos,ypos,j);
    }
}

void
read_plot_list(char *fname)
{
  /*
    AKL This differs from megsurfer.c
    The plot list has a label associated with each vertex number
  */
  
  int j,vnum;
  FILE *fp;
  
  printf("read_plot_list(%s)\n",fname);
  fp = fopen(fname,"r");
  if (fp==NULL) {printf("surfer: ### can't find file %s\n",fname);PR return;}
  fscanf(fp,"%d",&sol_nplotlist);
  for (j=0;j<sol_nplotlist;j++)
    {
      fscanf(fp,"%d",&vnum);
      find_nearest_nonzero(vnum,&sol_plotlist[j]);
    }
  fclose(fp);
}

void
read_vertex_list(char *fname)
{
  int j;
  FILE *fp;
  
  printf("read_vertex_list(%s)\n",fname);
  fp = fopen(fname,"r");
  if (fp==NULL) {printf("surfer: ### can't find file %s\n",fname);PR return;}
  fscanf(fp,"%d",&vertex_nplotlist);
  printf("  vertex_nplotlist = %d\n  ",vertex_nplotlist);
  for (j=0;j<vertex_nplotlist;j++)
    {
      fscanf(fp,"%d",&vertex_plotlist[j]);
      printf("%d ",vertex_plotlist[j]);
    }
  fclose(fp);
  printf("\n");
}

void
plot_nearest_nonzero(int vnum)
{
  int imin;
  
  find_nearest_nonzero(vnum,&imin);
  plot_sol_time_course(imin,0,0,0,0,0);
}

void
find_nearest_nonzero(int vnum, int *indxptr)
{
  int i,imin;
  float d,mindist;
  
  imin = 0 ;
  printf("find_nearest_nonzero(%d,*)\n",vnum);
  mindist = 1000000;
  for (i=0;i<sol_nnz;i++)
    {
      d = SQR(mris->vertices[sol_dipindex[i]].x-mris->vertices[vnum].x)+
  SQR(mris->vertices[sol_dipindex[i]].y-mris->vertices[vnum].y)+
  SQR(mris->vertices[sol_dipindex[i]].z-mris->vertices[vnum].z);
      if (d<mindist) {mindist=d; imin=i;}
    }
  printf("%d => %d (%d) (mindist = %f)\n",
   vnum,sol_dipindex[imin],imin,sqrt(mindist));
  *indxptr = imin;
}

void
find_nearest_fixed(int vnum, int *vnumptr)
{
  int i,imin= -1;
  float d,mindist;
  
  printf("find_nearest_fixed(%d,*)\n",vnum);
  mindist = 1000000;
  for (i=0;i<mris->nvertices;i++)
    if (mris->vertices[i].fixedval)
      {
  d = SQR(mris->vertices[i].x-mris->vertices[vnum].x)+
    SQR(mris->vertices[i].y-mris->vertices[vnum].y)+
    SQR(mris->vertices[i].z-mris->vertices[vnum].z);
  if (d<mindist) {mindist=d; imin=i;}
      }
  *vnumptr = imin;
  printf("selection = %d\n",imin);
}

void
plot_sol_time_course(int imin, int plot_xdim, int plot_ydim, int plot_xloc,
         int plot_yloc, int plotnum)
{
  int i,k;
  float val;
  FILE *fp;
  char fname[STRLEN],command[STRLEN],cmd[STRLEN];
  
  printf("plot_sol_time_course(%d,%d,%d,%d,%d)\n",
   imin,plot_xdim,plot_ydim,plot_xloc,plot_yloc);
  sprintf(command,"xplot");
  for (i=0;i<sol_nrec;i++)
    {
      sprintf(fname,"/tmp/tmp-%d-%d.xplot",plotnum,i);
      sprintf(cmd,"%s %s",command,fname);
      sprintf(command,"%s",cmd);
      fp = fopen(fname,"w");
      if (fp==NULL){printf("surfer: ### can't create file %s\n",fname);PR return;}
      
      fprintf(fp,"/color %d\n",i+1);
      
      fprintf(fp,"/plotname %d\n",i);
      fprintf(fp,"/ytitle %d\n",plotnum);
      if (plot_xdim>0)
  fprintf(fp,"/geometry %dx%d+%d+%d\n",
    plot_xdim,plot_ydim,plot_xloc,plot_yloc);
      for (k=0;k<sol_ntime;k++)
  {
    val = dipval(i,imin,k);
    fprintf(fp,"%f %f\n",sol_lat[k],val);
  }
      fclose(fp);
    }
  sprintf(cmd,"%s &\n",command);
  sprintf(command,"%s",cmd);
  system(command);
}

void
load_vals_from_sol(float tmid, float dt, int num)
{
  int j,k,ilat0,ilat1;
  double avgval,nsum,val,f;
  
  printf("load_vals_from_sol(%f,%f,%d)\n",tmid,dt,num);
  for (k=0;k<mris->nvertices;k++)
    {
      mris->vertices[k].val = 0;
      if (!mris->vertices[k].fixedval)
  mris->vertices[k].undefval = TRUE;
    }
  ilat0 = floor((tmid-dt/2-sol_epoch_begin_lat)/sol_sample_period+0.5);
  ilat1 = floor((tmid+dt/2-sol_epoch_begin_lat)/sol_sample_period+0.5);
  printf("ilat0=%d, ilat1=%d\n",ilat0,ilat1);
  nsum = ilat1-ilat0+1;
  for (k=0;k<sol_nnz;k++)
    {
      avgval = 0;
      for (j=ilat0;j<=ilat1;j++)
  {
    val = dipval(num,k,j);
    avgval += val;
  }
      f = fabs(sol_pval[k]);
      if (sol_pslope!=0)
  f = tanh(sol_pslope*(f-sol_pthresh));
      else
  f = 1;
      /*
  if (f<0) f = 0;
      */
      mris->vertices[sol_dipindex[k]].val = f*avgval/nsum;
    }
}

void
load_var_from_sol(int num)
{
  int j,k,ilat2;
  double val,maxval,f;
  
  printf("load_var_from_sol(%d)\n",num);
  for (k=0;k<mris->nvertices;k++)
    {
      mris->vertices[k].val = 0;
      if (!mris->vertices[k].fixedval)
  mris->vertices[k].undefval = TRUE;
    }
  ilat2 = floor((0-sol_epoch_begin_lat)/sol_sample_period+0.5);
  for (k=0;k<sol_nnz;k++)
    {
      maxval = 0;
      for (j=ilat2;j<sol_ntime;j++)
  {
    val = fabs(dipval(num,k,j));
    if (val>maxval) maxval = val;
  }
      f = fabs(sol_pval[k]);
      if (sol_pslope!=0)
  f = tanh(sol_pslope*(f-sol_pthresh));
      else
  f = 1;
      /*
  if (f<0) f = 0;
      */
      mris->vertices[sol_dipindex[k]].val = f*maxval;
    }
}

void
variance_ratio(void)
{
  int j,k,ilat2;
  double sum0,sum1,nsum,val;
  
  printf("variance_ratio()\n");
  for (k=0;k<mris->nvertices;k++)
    {
      mris->vertices[k].val = 0;
      if (!mris->vertices[k].fixedval)
  mris->vertices[k].undefval = TRUE;
    }
  ilat2 = floor((0-sol_epoch_begin_lat)/sol_sample_period+0.5);
  for (k=0;k<sol_nnz;k++)
    {
      sum0 = sum1 = 0;
      nsum = 0;
      for (j=ilat2;j<sol_ntime;j++)
  {
    val = dipval(0,k,j);
    sum0 += SQR(val);
    val = dipval(1,k,j);
    sum1 += SQR(val);
    nsum++;
  }
      if (sum0==0 || sum1==0) sum0 = sum1 = 1;
      mris->vertices[sol_dipindex[k]].val = 10*log10(sum0/sum1);
    }
}

void
load_pvals(void)
{
  int k;
  double f;
  
  printf("load_pvals()\n");
  for (k=0;k<mris->nvertices;k++)
    {
      mris->vertices[k].val = 0;
      if (!mris->vertices[k].fixedval)
  mris->vertices[k].undefval = TRUE;
    }
  for (k=0;k<sol_nnz;k++)
    {
      f = sol_pval[k];
      if (sol_rectpval)
  f = fabs(f);
      mris->vertices[sol_dipindex[k]].val = f;
    }
}

void
compute_pval_fwd(float pthresh)
{
#if 0
  int j,k;
  float f;
  
  for (k=0;k<mris->nvertices;k++)
    {
      mris->vertices[k].val = 0;
      if (!mris->vertices[k].fixedval)
  mris->vertices[k].undefval = TRUE;
    }
  for (j=0;j<sol_nchan;j++)
    sol_sensval[j] = 0;
  for (k=0;k<sol_nnz;k++)
    {
      f = sol_pval[k];
      if ((pthresh>=0 && f<pthresh) || (pthresh<0 && -f<-pthresh)) f = 0;
      if (f<0) f = -f;
      mris->vertices[sol_dipindex[k]].val = f;
      if (f>0)
  {
    if (sol_nperdip==1)
      for (j=0;j<sol_nchan;j++)
        sol_sensval[j] += f*sol_A[j][sol_nperdip*k];
    else
      for (j=0;j<sol_nchan;j++)
        sol_sensval[j] += f*sol_A[j][sol_nperdip*k+0]*mris->vertices[sol_dipindex[k]].dipnx +
    f*sol_A[j][sol_nperdip*k+1]*mris->vertices[sol_dipindex[k]].dipny +
    f*sol_A[j][sol_nperdip*k+2]*mris->vertices[sol_dipindex[k]].dipnz;
  }
    }
#endif
}

void
compute_select_fwd(float maxdist)
{
#if 0
  int j,k;
  float dist;
  
  for (j=0;j<sol_nchan;j++)
    sol_sensval[j] = 0;
  for (k=0;k<mris->nvertices;k++)
    {
      mris->vertices[k].val = 0;
      if (!mris->vertices[k].fixedval)
  mris->vertices[k].undefval = TRUE;
    }
  find_nearest_fixed(selection,&selection);
  for (k=0;k<sol_nnz;k++)
    {
      dist = sqrt(SQR(mris->vertices[sol_dipindex[k]].x-mris->vertices[selection].x)+
      SQR(mris->vertices[sol_dipindex[k]].y-mris->vertices[selection].y)+
      SQR(mris->vertices[sol_dipindex[k]].z-mris->vertices[selection].z));
      if (dist<=maxdist)
  {
    mris->vertices[sol_dipindex[k]].val = 1;
    if (sol_nperdip==1)
      for (j=0;j<sol_nchan;j++)
        sol_sensval[j] += sol_A[j][sol_nperdip*k];
    else
      for (j=0;j<sol_nchan;j++)
        sol_sensval[j] += sol_A[j][sol_nperdip*k+0]*mris->vertices[sol_dipindex[k]].dipnx +
    sol_A[j][sol_nperdip*k+1]*mris->vertices[sol_dipindex[k]].dipny +
    sol_A[j][sol_nperdip*k+2]*mris->vertices[sol_dipindex[k]].dipnz;
  }
    }
#endif
}

void
compute_select_crosstalk(void)
{
#if 0
  int j,k,l,lc;
  float dist,maxdist=1e-10;
  double sum,xtalk,xtalk0,sumwtdist,sumwt,hvol;
  
  for (j=0;j<sol_nchan;j++)
    sol_sensval[j] = 0;
  for (k=0;k<mris->nvertices;k++)
    {
      mris->vertices[k].val = 0;
      if (!mris->vertices[k].fixedval)
  mris->vertices[k].undefval = TRUE;
    }
  find_nearest_fixed(selection,&selection);
  sumwtdist = sumwt = hvol = 0;
  for (k=0;k<sol_nnz;k++)
    {
      dist = sqrt(SQR(mris->vertices[sol_dipindex[k]].x-mris->vertices[selection].x)+
      SQR(mris->vertices[sol_dipindex[k]].y-mris->vertices[selection].y)+
      SQR(mris->vertices[sol_dipindex[k]].z-mris->vertices[selection].z));
      if (dist<=maxdist)
  {
    for (l=0;l<sol_nnz;l++)
      {
        xtalk = 0;
        for (lc=0;lc<sol_nperdip;lc++)
    {
      sum = 0;
      for (j=0;j<sol_nchan;j++)
        sum += sol_W[k*sol_nperdip+lc][j]*sol_A[j][sol_nperdip*l+lc];
      xtalk += sum*sum;
    }
        mris->vertices[sol_dipindex[l]].val = xtalk;
      }
    xtalk0 = mris->vertices[sol_dipindex[k]].val;
    for (l=0;l<sol_nnz;l++)
      {
        xtalk = mris->vertices[sol_dipindex[l]].val /= xtalk0; /* normalize by auto-crosstalk */
        dist = sqrt(SQR(mris->vertices[sol_dipindex[l]].dipx-mris->vertices[sol_dipindex[k]].dipx)+
        SQR(mris->vertices[sol_dipindex[l]].dipy-mris->vertices[sol_dipindex[k]].dipy)+
        SQR(mris->vertices[sol_dipindex[l]].dipz-mris->vertices[sol_dipindex[k]].dipz));
        sumwtdist += xtalk*dist;
        sumwt += xtalk;
        if (xtalk>0.5) hvol++;
      }
  }
    }
  printf("mean half-max area: %f, xtalk-weighted dist: %f,  mean xtalk: %f)\n",
   hvol/sol_nnz*mris->total_area,sumwtdist/sumwt,sqrt(sumwt/sol_nnz));
#endif
}

#if 0
void
compute_all_crosstalk(void)
{
  int i,j,k,l,lc;
  float dist,maxdist=1e-10;
  double sum,xtalk,xtalk0,sumwtdist,sumwt,hvol,sumhvol,sumxtalk,nsum;
  
  printf("compute_all_crosstalk()\n");
  
  for (j=0;j<sol_nchan;j++)
    sol_sensval[j] = 0;
  for (k=0;k<mris->nvertices;k++)
    {
      mris->vertices[k].val = 0;
      if (!mris->vertices[k].fixedval)
  mris->vertices[k].undefval = TRUE;
    }
  sumhvol = nsum = 0;
  for (k=0;k<sol_nnz;k++)
    {
      printf("\rcountdown %4d  ",sol_nnz-k);fflush(stdout);
      sumwtdist = sumwt = hvol = 0;
      for (l=0;l<sol_nnz;l++)
  {
    xtalk = 0;
    for (lc=0;lc<sol_nperdip;lc++)
      {
        sum = 0;
        for (j=0;j<sol_nchan;j++)
    sum += sol_W[k*sol_nperdip+lc][j]*sol_A[j][sol_nperdip*l+lc];
        xtalk += sum*sum;
      }
    mris->vertices[sol_dipindex[l]].val2 = xtalk;
  }
      xtalk0 = mris->vertices[sol_dipindex[k]].val2;
      for (l=0;l<sol_nnz;l++)
  {
    xtalk = mris->vertices[sol_dipindex[l]].val2 /= xtalk0; /* normalize by auto-cross
                     talk */
    sumwt += xtalk;
    if (xtalk>0.5) hvol++;
  }
      mris->vertices[sol_dipindex[k]].val = hvol/sol_nnz*mris->total_area;
      sumxtalk += sumwt/sol_nnz;
      sumhvol += hvol;
      nsum++;
    }
  printf("\n");
  sumhvol /= nsum;
  sumxtalk /= nsum;
  printf("mean half-max area: %f, mean xtalk: %f)\n",
   sumhvol/sol_nnz*mris->total_area,sqrt(sumxtalk));
}
#endif

void
compute_all_crosstalk(int weighttype)
{
#if 0
  int j,k,l,lc;
  float dist;
  double sum,xtalk,xtalk0,sumwtxtalk,sumwt,sumxtalk;
  
  printf("compute_all_crosstalk()\n");
  
  for (j=0;j<sol_nchan;j++)
    sol_sensval[j] = 0;
  for (k=0;k<mris->nvertices;k++)
    {
      mris->vertices[k].val = 0;
      if (!mris->vertices[k].fixedval)
  mris->vertices[k].undefval = TRUE;
    }
  for (k=0;k<sol_nnz;k++)
    {
      printf("\rcountdown %4d  ",sol_nnz-k);fflush(stdout);
      sumwtxtalk = sumxtalk = sumwt = 0;
      for (l=0;l<sol_nnz;l++)
  {
    xtalk = 0;
    for (lc=0;lc<sol_nperdip;lc++)
      {
        sum = 0;
        for (j=0;j<sol_nchan;j++)
    sum += sol_W[k*sol_nperdip+lc][j]*sol_A[j][sol_nperdip*l+lc];
        xtalk += sum*sum;
      }
    mris->vertices[sol_dipindex[l]].val2 = xtalk;
  }
      if (weighttype==0)
  {
    xtalk0 = mris->vertices[sol_dipindex[k]].val2;
    for (l=0;l<sol_nnz;l++)
      {
        xtalk = mris->vertices[sol_dipindex[l]].val2 /= xtalk0; /* normalize by auto-crosstalk */
        wt = 1;
        sumwt += wt;
        sumxtalk += xtalk;
        sumwtxtalk += wt*xtalk;
      }
  }
      else if (weighttype==1)
  {
    xtalk0 = mris->vertices[sol_dipindex[k]].val2;
    for (l=0;l<sol_nnz;l++)
      {
        dist = sqrt(SQR(mris->vertices[sol_dipindex[l]].dipx-mris->vertices[sol_dipindex[k]].dipx)+
        SQR(mris->vertices[sol_dipindex[l]].dipy-mris->vertices[sol_dipindex[k]].dipy)+
        SQR(mris->vertices[sol_dipindex[l]].dipz-mris->vertices[sol_dipindex[k]].dipz));
        xtalk = mris->vertices[sol_dipindex[l]].val2 /= xtalk0; /* normalize by auto-crosstalk */
        wt = dist;
        sumwt += wt;
        sumxtalk += xtalk;
        sumwtxtalk += wt*xtalk;
      }
  }
      else if (weighttype==2)
  {
    xtalk0 = mris->vertices[sol_dipindex[k]].val2;
    for (l=0;l<sol_nnz;l++)
      {
        dist = sqrt(SQR(mris->vertices[sol_dipindex[l]].dipx-mris->vertices[sol_dipindex[k]].dipx)+
        SQR(mris->vertices[sol_dipindex[l]].dipy-mris->vertices[sol_dipindex[k]].dipy)+
        SQR(mris->vertices[sol_dipindex[l]].dipz-mris->vertices[sol_dipindex[k]].dipz));
        xtalk = mris->vertices[sol_dipindex[l]].val2 /= xtalk0; /* normalize by auto-crosstalk */
        wt = dist;
        sumwt += xtalk;
        sumxtalk += xtalk;
        sumwtxtalk += wt*xtalk;
      }
  }
      mris->vertices[sol_dipindex[k]].val = sumwtxtalk/sumwt;
    }
  printf("\n");
#endif
}

void
compute_select_pointspread(void)
{
#if 0
  int j,k,l,lc;
  float dist,maxdist=1e-10;
  double sum,xtalk,xtalk0,sumwtdist,sumwt,hvol;
  
  for (j=0;j<sol_nchan;j++)
    sol_sensval[j] = 0;
  for (k=0;k<mris->nvertices;k++)
    {
      mris->vertices[k].val = 0;
      if (!mris->vertices[k].fixedval)
  mris->vertices[k].undefval = TRUE;
    }
  find_nearest_fixed(selection,&selection);
  sumwtdist = sumwt = hvol = 0;
  for (k=0;k<sol_nnz;k++)
    {
      dist = sqrt(SQR(mris->vertices[sol_dipindex[k]].x-mris->vertices[selection].x)+
      SQR(mris->vertices[sol_dipindex[k]].y-mris->vertices[selection].y)+
      SQR(mris->vertices[sol_dipindex[k]].z-mris->vertices[selection].z));
      if (dist<=maxdist)
  {
    for (l=0;l<sol_nnz;l++)
      {
        xtalk = 0;
        for (lc=0;lc<sol_nperdip;lc++)
    {
      sum = 0;
      for (j=0;j<sol_nchan;j++)
        sum += sol_W[l*sol_nperdip+lc][j]*sol_A[j][sol_nperdip*k+lc];
      xtalk += sum*sum;
    }
        mris->vertices[sol_dipindex[l]].val = xtalk;
      }
    xtalk0 = mris->vertices[sol_dipindex[k]].val;
    for (l=0;l<sol_nnz;l++)
      {
        xtalk = mris->vertices[sol_dipindex[l]].val;
        dist = sqrt(SQR(mris->vertices[sol_dipindex[l]].dipx-mris->vertices[sol_dipindex[k]].dipx)+
        SQR(mris->vertices[sol_dipindex[l]].dipy-mris->vertices[sol_dipindex[k]].dipy)+
        SQR(mris->vertices[sol_dipindex[l]].dipz-mris->vertices[sol_dipindex[k]].dipz));
        sumwtdist += xtalk*dist*dist;
        xtalk /= xtalk0; /* normalize by auto-crosstalk */
        sumwt += xtalk;
        if (xtalk>0.5) hvol++;
        mris->vertices[sol_dipindex[l]].val = xtalk;
      }
  }
    }
  printf("mean half-max area: %f, mean pointspread: %f, pointspread-weighted dist2: %f)\n",
   hvol/sol_nnz*mris->total_area,sqrt(sumwt/sol_nnz),sumwtdist/sumwt);
#endif
}

void
compute_all_pointspread(void)
{
#if 0
  int i,j,k,l,lc;
  float dist2;
  double sum,xtalk,xtalk0,sumwtdist,sumwt,hvol,sumhvol,sumxtalk,totwtdist,nsum;
  
  printf("compute_all_crosstalk()\n");
  /*
    for (k=0;k<sol_nnz;k++)
    for (kc=0;kc<sol_nperdip;kc++)
    {
    sum = 0;
    for (i=0;i<sol_nchan;i++)
    for (l=0;l<sol_nnz;l++)
    for (lc=0;lc<sol_nperdip;lc++)
    sum += sol_W[k*sol_nperdip+kc][i]*sol_A[i][l*sol_nperdip+lc];
    printf("%d(%d): area = %f\n",k,kc,sum);
    }
  */
  printf("AW\n");
  for (i=0;i<10;i++)
    {
      for (j=0;j<10;j++)
  {
    sum = 0;
    for (l=0;l<sol_nnz;l++)
      for (lc=0;lc<sol_nperdip;lc++)
        sum += sol_A[i][l*sol_nperdip+lc]*sol_W[l*sol_nperdip+lc][j];
    printf("%9.1e",sum);
  }
      printf("\n");
    }
  printf("A\n");
  for (i=0;i<10;i++)
    {
      for (j=0;j<10;j++)
  {
    printf("%9.1e",sol_A[i][j]);
  }
      printf("\n");
    }
  printf("W\n");
  for (i=0;i<10;i++)
    {
      for (j=0;j<10;j++)
  {
    printf("%9.1e",sol_W[i][j]);
  }
      printf("\n");
    }
  return;
  
  for (j=0;j<sol_nchan;j++)
    sol_sensval[j] = 0;
  for (k=0;k<mris->nvertices;k++)
    {
      mris->vertices[k].val = 0;
      if (!mris->vertices[k].fixedval)
  mris->vertices[k].undefval = TRUE;
    }
  sumhvol = nsum = totwtdist = 0;
  for (k=0;k<sol_nnz;k++)
    {
      printf("\rcountdown %4d  ",sol_nnz-k);fflush(stdout);
      sumwtdist = sumwt = hvol = 0;
      for (l=0;l<sol_nnz;l++)
  {
    xtalk = 0;
    for (lc=0;lc<sol_nperdip;lc++)
      {
        sum = 0;
        for (j=0;j<sol_nchan;j++)
    sum += sol_W[l*sol_nperdip+lc][j]*sol_A[j][sol_nperdip*k+lc];
        xtalk += sum*sum;
      }
    mris->vertices[sol_dipindex[l]].val2 = xtalk;
  }
      xtalk0 = mris->vertices[sol_dipindex[k]].val2;
      for (l=0;l<sol_nnz;l++)
  {
    dist2 = SQR(mris->vertices[sol_dipindex[l]].dipx-mris->vertices[sol_dipindex[k]].dipx)+
      SQR(mris->vertices[sol_dipindex[l]].dipy-mris->vertices[sol_dipindex[k]].dipy)+
      SQR(mris->vertices[sol_dipindex[l]].dipz-mris->vertices[sol_dipindex[k]].dipz);
    xtalk = mris->vertices[sol_dipindex[l]].val2;
    sumwtdist += dist2*xtalk;
    xtalk /= xtalk0; /* normalize by auto-crosstalk */
    sumwt += xtalk;
    if (xtalk>0.5) hvol++;
  }
      mris->vertices[sol_dipindex[k]].val = hvol/sol_nnz*mris->total_area;
      sumxtalk += sumwt/sol_nnz;
      totwtdist += sumwtdist/sol_nnz;
      sumhvol += hvol;
      nsum++;
    }
  printf("\n");
  sumhvol /= nsum;
  sumxtalk /= nsum;
  totwtdist /= nsum;
  printf("mean half-max area: %f, mean pointspread: %f, mean wt. dist^2: %f)\n",
   sumhvol/sol_nnz*mris->total_area,sqrt(sumxtalk),totwtdist);
#endif
}

void
recompute_select_inverse(void)
{
#if 0
  int i,j,k,l,lc;
  float dist,maxdist=1e-10;
  double sum;
  /*
    if (sol_nperdip!=1) {printf("sol_nperdip must be 1!\n");return;}
  */
  find_nearest_fixed(selection,&selection);
  for (i=0;i<sol_nchan;i++)
    for (j=0;j<sol_nchan;j++)
      sol_M[i][j] = 0;
  for (k=0;k<sol_nnz;k++)
    {
      dist = sqrt(SQR(mris->vertices[sol_dipindex[k]].x-mris->vertices[selection].x)+
      SQR(mris->vertices[sol_dipindex[k]].y-mris->vertices[selection].y)+
      SQR(mris->vertices[sol_dipindex[k]].z-mris->vertices[selection].z));
      if (dist<=maxdist)
  {
    for (l=0;l<sol_nnz;l++)
      {
        dist = sqrt(SQR(mris->vertices[sol_dipindex[l]].dipx-mris->vertices[sol_dipindex[k]].dipx)+
        SQR(mris->vertices[sol_dipindex[l]].dipy-mris->vertices[sol_dipindex[k]].dipy)+
        SQR(mris->vertices[sol_dipindex[l]].dipz-mris->vertices[sol_dipindex[k]].dipz));
        for (i=0;i<sol_nchan;i++)
    for (j=0;j<sol_nchan;j++)
      for (lc=0;lc<sol_nperdip;lc++)
        sol_M[i][j] += sol_A[i][sol_nperdip*l+lc]*sol_A[j][sol_nperdip*l+lc]*sqrt(dist);
      }
    regularize_matrix(sol_M,sol_nchan,0.0001);
    inverse(sol_M,sol_Mi,sol_nchan);
    for (lc=0;lc<sol_nperdip;lc++)
      {
        for (i=0;i<sol_nchan;i++)
    sol_sensvec1[i] = sol_A[i][sol_nperdip*k+lc]; 
        vector_multiply(sol_Mi,sol_sensvec1,sol_sensvec2,sol_nchan,sol_nchan);
        sum = 0;
        for (i=0;i<sol_nchan;i++)
    sum += sol_sensvec1[i]*sol_sensvec2[i];
        if (sum!=0) sum = 1/sum;
        for (i=0;i<sol_nchan;i++)
    sol_W[sol_nperdip*k+lc][i] = sol_sensvec2[i]*sum; 
      }
  }
    }
#endif
}

void
compute_pval_inv(void)
{
#if 0
  int j,k,jc;
  double sum,sum2;
  
  printf("compute_pval_inv()\n");
  for (k=0;k<mris->nvertices;k++)
    {
      mris->vertices[k].val = 0;
      if (!mris->vertices[k].fixedval)
  mris->vertices[k].undefval = TRUE;
    }
  if (sol_plot_type==0)
    {
      for (j=0;j<sol_nnz;j++)
  {
    sum2 = 0;
    for (jc=0;jc<sol_nperdip;jc++)
      {
        sum = 0;
        for (k=0;k<sol_nchan;k++)
    sum += sol_W[j*sol_nperdip+jc][k]*sol_sensval[k];
        sum2 += sum*sum;
      }
    mris->vertices[sol_dipindex[j]].val = sqrt(sum2);
  }
    }
  if (sol_plot_type==1 || sol_plot_type==2)
    {
      for (j=0;j<sol_nnz;j++)
  {
    sum2 = 0;
    for (jc=0;jc<sol_nperdip;jc++)
      {
        sum = 0;
        for (k=0;k<sol_nchan;k++)
    sum += sol_W[j*sol_nperdip+jc][k]*sol_sensval[k];
        if (sol_nperdip==1)
    sum2 += sum;
        else
    {
      if (jc==0)
        sum2 += sum*mris->vertices[sol_dipindex[j]].dipnx;
      else if (jc==1)
        sum2 += sum*mris->vertices[sol_dipindex[j]].dipny;
      else if (jc==2)
        sum2 += sum*mris->vertices[sol_dipindex[j]].dipnz;
    }
      }
    mris->vertices[sol_dipindex[j]].val = sum2;
  }
    }
  normalize_vals();
#endif
}

void
normalize_vals(void)
{
  int k;
  float val,maxval=0;
  
  for (k=0;k<mris->nvertices;k++)
    {
      val = fabs(mris->vertices[k].val);
      if (val>maxval) maxval = val;
    }
  if (maxval!=0)
    for (k=0;k<mris->nvertices;k++)
      mris->vertices[k].val /= maxval;
}

void
normalize_time_courses(int normtype)
{
  double val,sum,sum2,nsum,noise_rms,maxval;
  int i,j,k,ic,ilat0,ilat1,ilat2;
  FLOATTYPE tmpvec1[2048];
  
  sum = maxval = 0.0f ;
  printf("normalize_time_courses(%d)\n",normtype);
  if (normtype==0)
    {
      for (i=0;i<sol_nnz;i++)
  sol_dipfact[i] = 1.0;
    }
  else if (normtype==1) /* normalize for baseline noise covariance */
    {
      ilat1 = floor((sol_baseline_end-sol_epoch_begin_lat)/sol_sample_period+0.5);
      ilat0 = floor(ilat1-sol_baseline_period/sol_sample_period+0.5);
      ilat2 = floor((0-sol_epoch_begin_lat)/sol_sample_period+0.5);
      for (k=0;k<sol_nnz;k++)
  {
    sol_dipfact[k] = 1;
    sum2 = 0;
    nsum = 0;
    for (i=0;i<sol_nrec;i++)
      {
        for (j=ilat0;j<=ilat1;j++)
    {
      val = dipval(i,k,j);
      sum2 += SQR(val);
      val = fabs(val);
      if (val>maxval) maxval=val;
      nsum++;
    }
      }
    noise_rms = sqrt(sum2/nsum);
    maxval = 0;
    for (i=0;i<sol_nrec;i++)
      {
        for (j=ilat2;j<sol_ntime;j++)
    {
      val = dipval(i,k,j);
      val = fabs(val);
      if (val>maxval) maxval=val;
    }
      }
    sol_dipfact[k] = 1.0/(noise_rms);
  }
    } else if (normtype==2) /* normalize for white noise w.w = 1 */
      {
  for (i=0;i<sol_nnz;i++)
    {
      sum2 = 0;
      for (ic=0;ic<sol_nperdip;ic++)
        {
    for (j=0;j<sol_nchan;j++)
      {
        sum = 0;
        for (k=0;k<sol_nchan;k++)
          sum += SQR(sol_W[i*sol_nperdip+ic][k]);
      }
    sum2 += sum;
        }
      noise_rms = sqrt(sum2);
      sol_dipfact[i] = 1.0/noise_rms;
    }
      } else if (normtype==3) /* normalize by read noise covariance matrix */
  {
    for (i=0;i<sol_nnz;i++)
      {
        sum2 = 0;
        for (ic=0;ic<sol_nperdip;ic++)
    {
      for (j=0;j<sol_nchan;j++)
        {
          sum = 0;
          for (k=0;k<sol_nchan;k++)
      sum += sol_W[i*sol_nperdip+ic][k]*sol_NoiseCovariance[j][k];
          tmpvec1[j] = sum;
        }
      sum = 0;
      for (j=0;j<sol_nchan;j++)
        sum += tmpvec1[j]*sol_W[i*sol_nperdip+ic][j];
      sum2 += sum;
    }
        noise_rms = sqrt(sum2);
        sol_dipfact[i] = 1.0/noise_rms;
        /*
    printf("sol_dipfact[%d] = %e\n",i,sol_dipfact[i]);
        */
      }
  } else if (normtype==4) /* "Unity Gain  wa = 1 */
    {
      for (i=0;i<sol_nnz;i++)
        {
    for (ic=0;ic<sol_nperdip;ic++)
      {
        sum = 0;
        for (k=0;k<sol_nchan;k++)
          sum += sol_W[i*sol_nperdip+ic][k]*sol_A[k][i*sol_nperdip+ic];
        if (sum!=0)
          sum = 1/sum;
        for (k=0;k<sol_nchan;k++)
          sol_W[i*sol_nperdip+ic][k] *= sum;
      }
    sol_dipfact[i] = 1.0;
        }
    }
  for (i=0;i<sol_nnz;i++)
    {
      for (ic=0;ic<sol_nperdip;ic++)
  for (k=0;k<sol_nchan;k++)
    sol_W[i*sol_nperdip+ic][k] *= sol_dipfact[i];
    }
}

void
normalize_inverse(void)
{
  int j,k,jc;
  double sum;
  
  printf("normalize_inverse()\n");
  for (j=0;j<sol_nnz;j++)
    {
      sum = 0;
      for (jc=0;jc<sol_nperdip;jc++)
  for (k=0;k<sol_nchan;k++)
    sum += SQR(sol_W[j*sol_nperdip+jc][k]);
      sum = sqrt(sum);
      for (jc=0;jc<sol_nperdip;jc++)
  for (k=0;k<sol_nchan;k++)
    sol_W[j*sol_nperdip+jc][k] /= sum;
    }
}

void
setsize_window(int pix)
{
  if (openglwindowflag) {
    printf("surfer: ### setsize_window failed: gl window already open\n");PR
                      return; }
  
  frame_xdim = (long)pix;
  frame_ydim = (long)pix;
}

void
resize_window(int pix)
{
  if (!openglwindowflag) {
    printf("surfer: ### resize_window failed: no gl window open\n");PR
                      return; }
  
#ifdef OPENGL
  if (renderoffscreen1) {
    printf("surfer: ### resize_window failed: can't resize offscreen win\n");PR
                         printf("surfer: ### use setsize_window <pix> before open_window\n");PR
                                         return; }
  
  if (pix>0) {  /* command line (not mouse) resize */
    XResizeWindow(xDisplay, w.wMain, pix, pix);
    if (TKO_HAS_OVERLAY(w.type))
      XResizeWindow(xDisplay, w.wOverlay, pix, pix);
    w.w = w.h = pix;
  }
  frame_xdim = w.w;
  frame_ydim = w.h;
  glViewport(0, 0, frame_xdim, frame_ydim); 
  resize_buffers(frame_xdim, frame_ydim);
  
#else
  if (pix>0) {   /* tcl: zeropix -> flag to reshape after mouse resize */
    prefposition(0,(short)pix,0,(short)pix);
    winconstraints();
    reshapeviewport();
    getsize(&frame_xdim,&frame_ydim);
    keepaspect(1,1);
    winconstraints();  /* call again to keep resizable */
    resize_buffers(frame_xdim,frame_ydim);
  }
  else {  /* tcl: zeropix flag->reshape w/mouse resize (REDRAW event) */
    reshapeviewport();
    getsize(&frame_xdim,&frame_ydim);
    resize_buffers(frame_xdim,frame_ydim);
  }
#endif
}

void
save_rgb(char *fname)
{
#if 1
  unsigned short *red, *blue, *green ;
  int             width, height, size ;
  
  width = (int)frame_xdim;
  height = (int)frame_ydim;
  size = width*height;
  red = (unsigned short *)calloc(size, sizeof(unsigned short)) ;
  green = (unsigned short *)calloc(size, sizeof(unsigned short)) ;
  blue = (unsigned short *)calloc(size, sizeof(unsigned short)) ;
  grabPixels(frame_xdim, frame_ydim, red, green, blue) ;
  save_rgbfile(fname, frame_xdim, frame_ydim, red, green, blue) ;
  free(blue) ; free(red) ; free(green) ;
#else
  if (!openglwindowflag) {
    printf("surfer: ### save_rgb failed: no gl window open\n");PR
                 return; }
  
  if (renderoffscreen1)
    pix_to_rgb(fname);
  else {
    if (scrsaveflag) { scrsave_to_rgb(fname); }
    else             { pix_to_rgb(fname);     }
  }
#endif
}

static void
move_window(int x,int y)
{
#ifdef OPENGL
  if (openglwindowflag) {
    XMoveWindow(xDisplay, w.wMain, x, y);
    w.x = x;
    w.y = y;
  }
  else if (!initpositiondoneflag) {
    tkoInitPosition(x,y,frame_xdim,frame_ydim);
    initpositiondoneflag = TRUE;
  }
  else ;
#endif
}

void
save_rgb_num(char *dir)
{
  char fname[NAME_LENGTH];
  FILE *fp;
  
  if (!openglwindowflag) {
    printf("surfer: ### save_rgb_num failed: no gl window open\n");PR
                     return; }
  
  fp = stdin;
  framenum = 0;
  while (fp!=NULL && framenum<999) { 
    framenum++;
    sprintf(fname,"%s/im-%03d.rgb",dir,framenum);
    fp = fopen(fname,"r");
    if (fp!=NULL) fclose(fp);
  }
  
  if (renderoffscreen1)
    pix_to_rgb(fname);
  else {
    if (scrsaveflag) { scrsave_to_rgb(fname); }
    else             { pix_to_rgb(fname);     }
  }
}

void
save_rgb_named_orig(char *dir, char *name)
{
  char fname[NAME_LENGTH];
  
  if (!openglwindowflag) {
    printf("surfer: ### save_rgb_named_orig failed: no gl window open\n");PR
                      return; }
  
  sprintf(fname,"%s/%s",dir,name);
  
  if (renderoffscreen1)
    pix_to_rgb(fname);
  else {
    if (scrsaveflag) { scrsave_to_rgb(fname); }
    else             { pix_to_rgb(fname);     }
  }
}

void
scrsave_to_rgb(char *fname)  /* about 2X faster than pix_to_rgb */
{
  char command[2*NAME_LENGTH];
  FILE *fp;
  long xorig,xsize,yorig,ysize;
  int x0,y0,x1,y1;
  
  getorigin(&xorig,&yorig);
  getsize(&xsize,&ysize);
  x0 = (int)xorig;  x1 = (int)(xorig+xsize-1);
  y0 = (int)yorig;  y1 = (int)(yorig+ysize-1);
  fp = fopen(fname,"w");
  if (fp==NULL) {printf("surfer: ### can't create file %s\n",fname);PR return;}
  fclose(fp);
  sprintf(command,"scrsave %s %d %d %d %d\n",fname,x0,x1,y0,y1);
  system(command);
  printf("surfer: file %s written\n",fname);PR
                }

void
pix_to_rgb(char *fname)
{
  
#ifdef OPENGL
  GLint swapbytes, lsbfirst, rowlength;
  GLint skiprows, skippixels, alignment;
  RGB_IMAGE *image;
  int y,width,height,size;
  unsigned short *r,*g,*b;
  unsigned short  *red, *green, *blue;
  FILE *fp;
  
  width = (int)frame_xdim;
  height = (int)frame_ydim;
  size = width*height;
  
  red = (unsigned short *)calloc(size, sizeof(unsigned short));
  green = (unsigned short *)calloc(size, sizeof(unsigned short));
  blue = (unsigned short *)calloc(size, sizeof(unsigned short));
  
  glGetIntegerv(GL_UNPACK_SWAP_BYTES, &swapbytes);
  glGetIntegerv(GL_UNPACK_LSB_FIRST, &lsbfirst);
  glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowlength);
  glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skiprows);
  glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skippixels);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
  
  glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  
  glReadPixels(0, 0, width, height, GL_RED,GL_UNSIGNED_SHORT, (GLvoid *)red);
  glReadPixels(0, 0, width, height, GL_GREEN,GL_UNSIGNED_SHORT,(GLvoid *)green);
  glReadPixels(0, 0, width, height, GL_BLUE,GL_UNSIGNED_SHORT, (GLvoid *)blue);
  
  glPixelStorei(GL_UNPACK_SWAP_BYTES, swapbytes);
  glPixelStorei(GL_UNPACK_LSB_FIRST, lsbfirst);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, rowlength);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, skiprows);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, skippixels);
  glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
  
  fp = fopen(fname,"w");
  if (fp==NULL){printf("surfer: ### can't create file %s\n",fname);PR return;}
  fclose(fp);
#ifdef IRIX
  image = iopen(fname,"w",RLE(1), 3, width, height, 3);
#else
  image = iopen(fname,"w",UNCOMPRESSED(1), 3, width, height, 3);
#endif
  if (!image)
    return ;
  for(y = 0 ; y < height; y++) {
    r = red + y * width;
    g = green + y * width;
    b = blue + y * width;
    putrow(image, r, y, 0);
    putrow(image, g, y, 1);
    putrow(image, b, y, 2);
  }
  iclose(image);
  free(red); free(green); free(blue);
  
  printf("surfer: file %s written\n",fname);PR
#else
                printf("surfer: ### pix_to_rgb implemented only in OpenGL version\n");PR
                                  return;
#endif
}

void
read_annotations(char *fname)
{
  /* begin rkt */
  /* now handled as importing into multiple labels. */
#if 1
  labl_import_annotation (fname);
#else
  if (MRISreadAnnotation(mris, fname) != NO_ERROR)
    return ;
  annotationloaded = TRUE ;
  surface_compiled = 0 ;
#endif
  /* end rkt */
}
void
read_annotated_image(char *fpref, int frame_xdim, int frame_ydim)
{
  char fname[NAME_LENGTH],command[NAME_LENGTH*2];
  FILE *fp;
  int i,j,k;
  
  sprintf(command,"tobin %s.rgb %s.bin\n",fpref,fpref);
  system(command);
  sprintf(fname,"%s.bin",fpref);
  fp = fopen(fname,"r");
  if (fp==NULL) {printf("surfer: ### File %s not found\n",fname);PR return;}
  k = fread(binbuff,3,frame_xdim*frame_ydim,fp);
  if (k != frame_xdim*frame_ydim) {
    printf("surfer: ### %s.rgb does not match current window size\n",fpref);PR
                        return;}
  fclose(fp);
  sprintf(command,"rm %s.bin\n",fpref);
  system(command);
  for (i=0;i<frame_ydim;i++)
    for (j=0;j<frame_xdim;j++)
      {
  /*
    framebuff[i*frame_xdim+j] = (binbuff[(i*frame_xdim+j)*3+2]<<16) |
    (binbuff[(i*frame_xdim+j)*3+1]<<8) |
    (binbuff[(i*frame_xdim+j)*3+0]);
  */
  framebuff[i*frame_xdim+j] = 
    (binbuff[(i*frame_xdim+j)+2*(frame_ydim*frame_xdim)]<<16) |
    (binbuff[(i*frame_xdim+j)+1*(frame_ydim*frame_xdim)]<<8) |
    (binbuff[(i*frame_xdim+j)+0]);
      }
  for (i=0;i<frame_ydim;i++)
    for (j=0;j<frame_xdim;j++)
      {
  binbuff[(i*frame_xdim+j)*3+2] = (framebuff[i*frame_xdim+j]&0xff0000)>>16;
  binbuff[(i*frame_xdim+j)*3+1] = (framebuff[i*frame_xdim+j]&0xff00)>>8;
  binbuff[(i*frame_xdim+j)*3+0] = (framebuff[i*frame_xdim+j]&0xff);
      }
  sprintf(fname,"%s.bin~",fpref);
  fp = fopen(fname,"w");
  if (fp==NULL){printf("surfer: ### can't create file %s\n",fname);PR return;} 
  fwrite(binbuff,3,frame_xdim*frame_ydim,fp);
  fclose(fp);
  sprintf(command,"frombin %s.bin~ %s.rgb~ %d %d 3 -i\n",
    fpref,fpref,frame_xdim,frame_ydim);
  system(command);
  sprintf(command,"rm %s.bin~\n",fpref);
  system(command);
}

int diff(int a,int b)
{
  int d;
  unsigned char a1,a2,a3,b1,b2,b3,d1,d2,d3;
  
  a1 = ((a>>16)&0xff);
  a2 = ((a>>8)&0xff);
  a3 = (a&0xff);
  b1 = ((b>>16)&0xff);
  b2 = ((b>>8)&0xff);
  b3 = (b&0xff);
  d1 = a1-b1;
  d2 = a2-b2;
  d3 = a3-b3;
  d = (((int)d1)<<16)|(((int)d2)<<8)|((int)d3);
  return d;
}

void
save_rgb_cmp_frame(char *dir, int ilat)    /* open/close version */
{
  int numframes,file_xdim,file_ydim;
  float flat;
  static float firstflat;
  static long beginlastframe;
  long xsize,ysize;
  short x0,y0,x1,y1;
  char fname[NAME_LENGTH];
  FILE *fp;
  
  if (renderoffscreen1) {
    printf("surfer: ### save_rgb_cmp_frame failed: TODO: offscreen\n");PR
                   return; }
  
  getsize(&xsize,&ysize);
  x0 = 0;
  y0 = 0;
  x1 = xsize-1;
  y1 = ysize-1;
  flat = (float)ilat;
  
  if (!openglwindowflag) {
    printf("surfer: ### save_rgb_cmp_frame failed: no gl window open\n");PR
                     return; }
  
  /* open next avail new file, else append (this session), get numframes */
  numframes = -1;  /* last==first: n frames written = n-1 images */
  if (!openedcmpfilenum) {
    fp = stdin;
    while (fp!=NULL && openedcmpfilenum<999) {
      openedcmpfilenum++;
      sprintf(fname,"%s/movie-%03d.cmp",dir,openedcmpfilenum);
      fp = fopen(fname,"r");
      if (fp!=NULL) fclose(fp); }
    fp = fopen(fname,"w");
    if (fp==NULL) {
      printf("surfer: ### can't create file %s\n",fname);PR
                 openedcmpfilenum = FALSE;
      return;}
    printf("surfer: opening new compressed movie:\n");
    printf("surfer:   %s\n",fname);
    numframes = 0;
    fwrite2((int)xsize,fp);
    fwrite2((int)ysize,fp);
    fwrite2(numframes,fp);
    lrectread(x0,y0,x1,y1,framebuff3);  /* save for later */
    firstflat = flat;
  }
  else { /* get frames written, reopen to append */
    sprintf(fname,"%s/movie-%03d.cmp",dir,openedcmpfilenum);
    fp = fopen(fname,"r");
    if (fp==NULL) {
      printf("surfer: ### File %s not found\n",fname);PR
              openedcmpfilenum = FALSE;
      return; }
    fread2(&file_xdim,fp); /* ignored */
    fread2(&file_ydim,fp); /* ignored */
    fread2(&numframes,fp);
    fclose(fp);
    fp = fopen(fname,"r+"); /* "r+": overwrite ("a": no backseek;"w": trunc) */
    if (fp==NULL){printf("surfer: ### can't create file %s\n",fname);PR return;}
  }
  
  /* write current frame over previous last (=first) frame */
  if (numframes > 0)
    fseek(fp,beginlastframe,SEEK_SET);
#if 0
  fwrite(&flat,1,sizeof(float),fp);
#else
  fwriteFloat(flat, fp) ;
#endif
  lrectread(x0,y0,x1,y1,framebuff);
  do_rgb_cmp_frame(xsize,ysize,fp);
  lrectread(x0,y0,x1,y1,framebuff2);
  numframes++;
  
  /* copy saved first frame (framebuff3) to last */
  beginlastframe = ftell(fp);
#if 0
  fwrite(&firstflat,1,sizeof(float),fp);
#else
  fwriteFloat(firstflat, fp) ;
#endif
  memcpy(framebuff,framebuff3,frame_xdim*frame_ydim*sizeof(long));
  do_rgb_cmp_frame(xsize,ysize,fp);
  /* don't save first=last frame to framebuff2 (because backup/overwrite) */
  fclose(fp);
  
  /* update num frames written */
  fp = fopen(fname,"r+");
  fseek(fp,4L,SEEK_SET);
  fwrite2(numframes,fp);
  fseek(fp,0L,SEEK_END);
  fclose(fp);
  printf("surfer: %s: frame %d done\n",fname,numframes+1);PR
                  }

/* open file return if already open */
void
open_rgb_cmp_named(char *dir, char *name)
{
  char str[NAME_LENGTH];
  
  if (renderoffscreen1) {
    printf("surfer: ### open_rgb_cmp_named failed: TODO: offscreen\n");PR
                   return; }
  if (!openglwindowflag) {
    printf("surfer: ### open_rgb_cmp_named failed: no gl window open\n");PR
                     return; }
  if (cmpfilenamedframe != -1) {
    printf("surfer: ### named compressed rgb movie already open\n");PR return;}
  else {
    sprintf(str,"%s/%s",dir,name);
    fpcmpfilenamed = fopen(str,"w");
    if (fpcmpfilenamed==NULL) {
      printf("surfer: ### can't create file %s\n",str);PR return;
    }
    cmpfilenamedframe = 0;
    printf("surfer: opening new compressed movie:\n");
    printf("surfer:   %s\n",str);PR
           }
  resize_buffers(frame_xdim, frame_ydim);
}

void
save_rgb_cmp_frame_named(float lat)
{
  long xsize,ysize;
  short x0,y0,x1,y1;
  
  if (renderoffscreen1) {
    printf("surfer: ### save_rgb_cmp_frame_named failed: TODO: offscreen\n");PR
                         return; }
  
  getsize(&xsize,&ysize);
  x0 = 0;
  y0 = 0;
  x1 = xsize-1;
  y1 = ysize-1;
  
  if (cmpfilenamedframe == -1) {
    printf("surfer: ### can't write frame: cmp movie file not opened yet\n");PR
                         return;
  }
  if (cmpfilenamedframe == 0) {  /* save 1st framebuff for end; write header */
    lrectread(x0,y0,x1,y1,framebuff3);
    fwrite2((int)xsize,fpcmpfilenamed);
    fwrite2((int)ysize,fpcmpfilenamed);
    fwrite2(cmpfilenamedframe,fpcmpfilenamed);  /* overwritten at end */
    cmpfilenamedfirstlat = lat;
  }
  
  /* write compressed frame */
#if 0
  fwrite(&lat,1,sizeof(float),fpcmpfilenamed);
#else
  fwriteFloat(lat, fpcmpfilenamed) ;
#endif
  lrectread(x0,y0,x1,y1,framebuff);
  do_rgb_cmp_frame(xsize,ysize,fpcmpfilenamed);
  lrectread(x0,y0,x1,y1,framebuff2);
  cmpfilenamedframe++;
  printf("surfer: cmp movie frame %d (lat=%3.3f) written\n",
   cmpfilenamedframe,lat);PR
          }

void
do_rgb_cmp_frame(long xsize,long ysize, FILE *fp)
{
  int i,lo,hi,pos,change;
  long fb1, fb2 ;
  
  lo = 0;
  hi = 0;
  pos = 0;
  while (lo<xsize*ysize)
    {
      fb1 = orderLongBytes(framebuff[hi])&0xffffff ;
      fb2 = orderLongBytes(framebuff2[hi])&0xffffff ;
      while ((hi<xsize*ysize)&&
       (hi-lo<32767)&&
       (fb1==fb2))
  {
    hi++;
    if (hi < xsize*ysize)
      {
        fb1 = orderLongBytes(framebuff[hi])&0xffffff ;
        fb2 = orderLongBytes(framebuff2[hi])&0xffffff ;
      }
  }
      if (hi>lo)
  {
    i = ((hi-lo)|0x8000);
    fwrite2(i,fp);
    pos += 2;
  }
      else
  {
    fb1 = orderLongBytes(framebuff[lo])&0xffffff ;
    fb2 = orderLongBytes(framebuff2[lo])&0xffffff ;
    change = diff(fb1,fb2);
    fb1 = orderLongBytes(framebuff[hi])&0xffffff ;
    fb2 = orderLongBytes(framebuff2[hi])&0xffffff ;
    while ((hi<xsize*ysize)&&
     (hi-lo<32767)&&
     (diff(fb1,fb2)==change))
      {
        hi++;
        if (hi < xsize*ysize)
    {
      fb1 = orderLongBytes(framebuff[hi])&0xffffff ;
      fb2 = orderLongBytes(framebuff2[hi])&0xffffff ;
    }
      }
    i = hi-lo;
    fwrite2(i,fp);
    fwrite3(change,fp);
    pos += 5;
  }
      lo = hi;
    }
}

void
close_rgb_cmp_named(void)      /* load first; save; write total frames */
{
  long xsize,ysize;
  short x0,y0,x1,y1;
  
  if (cmpfilenamedframe == -1) {
    printf("surfer: ### can't close_rgb_cmp_named: file not opened yet\n");PR
                       return; }
  
  getsize(&xsize,&ysize);
  x0 = 0;
  y0 = 0;
  x1 = xsize-1;
  y1 = ysize-1;
  
  /* write last compressed frame (same as first) */
#if 0
  fwrite(&cmpfilenamedfirstlat,1,sizeof(float),fpcmpfilenamed);
#else
  fwriteFloat(cmpfilenamedfirstlat,fpcmpfilenamed) ;
#endif
  memcpy(framebuff,framebuff3,frame_xdim*frame_ydim*sizeof(long));
  do_rgb_cmp_frame(xsize,ysize,fpcmpfilenamed);
  fseek(fpcmpfilenamed,4L,SEEK_SET);
  fwrite2(cmpfilenamedframe,fpcmpfilenamed);
  fseek(fpcmpfilenamed,0L,SEEK_END);  /* else file truncated! */
  fclose(fpcmpfilenamed);
  printf("surfer: closed compressed movie file\n");PR
                 cmpfilenamedframe = -1;
}

void
redraw(void)
{
  int i,navg;
  
  if (!openglwindowflag) {
    printf("surfer: ### redraw failed: no gl window open\n");PR
                     return; }
  
  if (overlayflag) { redraw_overlay(); return ; }
  
  czclear(BACKGROUND,getgconfig(GC_ZMAX));
  
  dipscale = 0;
  dipavg = dipvar = logaratavg = logaratvar = logshearavg = logshearvar = 0;
  navg = 0;
  for (i=0;i<mris->nvertices;i++)
    if (!mris->vertices[i].ripflag)
      {
  if (fabs(mris->vertices[i].curv)>dipscale)
    dipscale=fabs(mris->vertices[i].curv);
  dipavg += mris->vertices[i].curv;
  dipvar += SQR(mris->vertices[i].curv);
#if 0
  logaratavg += mris->vertices[i].logarat;
  logaratvar += SQR(mris->vertices[i].logarat);
  logshearavg += mris->vertices[i].logshear;
  logshearvar += SQR(mris->vertices[i].logshear);
#endif
  navg++;
      }
  dipavg /= navg;
  dipvar = sqrt(dipvar/navg - dipavg*dipavg);
  logaratavg /= navg;
  logaratvar = sqrt(logaratvar/navg - logaratavg*logaratavg);
  logshearavg /= navg;
  logshearvar = sqrt(logshearvar/navg - logshearavg*logshearavg);
  
  draw_surface();
  
  if (selection>=0) draw_cursor(selection,TRUE);
  
  /* begin rkt */
  draw_marked_vertices ();
  /* end rkt */

  if (doublebufferflag) swapbuffers();
  
  if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
    printf("surfer: dipscale=%f, dipavg=%f, dipvar=%f\n",
     dipscale,dipavg,dipvar);
  dipscale = (dipscale!=0)?1/dipscale:1.0;
  if (areaflag) {
    printf("surfer: logaratavg=%f, logaratvar=%f\n",logaratavg,logaratvar);PR
                       }
  if (flag2d) {
    printf("surfer: logshearavg=%f, logshearvar=%f\n",logshearavg,logshearvar);PR
                     }
}

void
redraw_second(void)
{
  int i,navg;
  
  if (!openglwindowflag) {
    printf("surfer: ### redraw_second failed: no gl window open\n");PR
                      return; }
  if (!secondsurfaceloaded) {
    printf("surfer: ### redraw_second failed: no second surface read\n");PR
                     return; }
  
  czclear(BACKGROUND,getgconfig(GC_ZMAX));
  
  dipavg2 = 0;
  navg = 0;
  for (i=0;i<mris2->nvertices;i++)
    if (!mris2->vertices[i].ripflag) {
      dipavg2 += mris2->vertices[i].curv;
      navg++;
    }
  dipavg2 /= navg;
  
  draw_second_surface();
  if (doublebufferflag) swapbuffers();
}
void
blinkbuffers(void)
{
  if (blinkflag) {
    if (blinkdelay<0) {
      if (doublebufferflag) swapbuffers();
#ifdef Irix
      sginap(blinktime);
#else
      sleep(blinktime);
#endif
    }
    else
      blinkdelay--;
  }
}

void
redraw_overlay(void)
{
  int i;
  float curvavg;
  
  czclear(BACKGROUND,getgconfig(GC_ZMAX));
  
  curvavg = 0;
  for (i=0;i<mris->nvertices;i++)
    curvavg += mris->vertices[i].curv;
  curvavg /= mris->nvertices;
  if (avgflag)
    {
      for (i=0;i<mris->nvertices;i++)
  mris->vertices[i].curv -= curvavg;
    }
  if (autoscaleflag)
    {
      dipscale = 0;
      for (i=0;i<mris->nvertices;i++)
  if (fabs(mris->vertices[i].val)>dipscale)
    dipscale=fabs(mris->vertices[i].val);
      printf("surfer: dipscale=%f\n",dipscale);PR
             dipscale = (dipscale!=0)?1/dipscale:1.0;
    }
  curvscale = 0;
  for (i=0;i<mris->nvertices;i++)
    if (fabs(mris->vertices[i].curv)>curvscale)
      curvscale=fabs(mris->vertices[i].curv);
  curvscale = (curvscale!=0)?1/curvscale:1.0;
  
  if (selection>=0) draw_cursor(selection,TRUE);
  
  draw_surface();
  
  if (doublebufferflag) swapbuffers();
  
  /*printf("curvscale=%f, curvavg=%f\n",curvscale,curvavg);PR*/
}


void
draw_cursor(int vindex,int onoroff)
{
  /* begin rkt */
# if 1
  if (onoroff)
    {
      RGBcolor (0, 255, 255);
      draw_vertex_hilite (vindex);
    }
  else
    {
      set_color (0, 0, GREEN_RED_CURV);
      draw_vertex_hilite (vindex);
    }
  
  return;
  
#else
  
  int i,k,n;
  FACE *f;
  VERTEX *v,*vselect;
  
  /* offscreen render opengl bug: RGBcolor(white) => surrounding faces color */
  
  if ((vindex > mris->nvertices)||(vindex<0)) {
    if (vindex != -1)
      printf ("surfer: ### vertex index %d out of bounds\n",vindex);
    return;
  }
  vselect = &mris->vertices[vindex];
  if (onoroff==FALSE)
    set_color(0.0,0.0,GREEN_RED_CURV);
  else {
    if (blackcursorflag)  RGBcolor(0,0,0);
    else                  RGBcolor(0,255,255);
  }
  
  for (i=0;i<vselect->vnum;i++)
    {
      v = &mris->vertices[vselect->v[i]];
      linewidth(CURSOR_LINE_PIX_WIDTH);
      bgnline();
      load_brain_coords(v->nx,v->ny,v->nz,v1);
      n3f(v1);
      load_brain_coords(v->x+cup*v->nx,v->y+cup*v->ny,v->z+cup*v->nz,v1);
      v3f(v1);
      load_brain_coords(vselect->nx,vselect->ny,vselect->nz,v1);
      n3f(v1);
      load_brain_coords(
      vselect->x+cup*v->nx,vselect->y+cup*v->ny,vselect->z+cup*v->nz,v1);
      v3f(v1);
      endline();
    }
  
  if (bigcursorflag) {
    for (k=0;k<vselect->num;k++) {
      bgnquadrangle();
      f = &mris->faces[vselect->f[k]];
      for (n=0;n<VERTICES_PER_FACE;n++) {
  v = &mris->vertices[f->v[n]];
  load_brain_coords(v->nx,v->ny,v->nz,v1);
  n3f(v1);
  load_brain_coords(v->x+cup*v->nx,v->y+cup*v->ny,v->z+cup*v->nz,v1);
  v3f(v1);
      }
      endpolygon();
    }
  }
  glFlush() ;
#endif
  /* end rkt */
}

void
draw_all_cursor(void)
{
  int j;
  
  for (j=0;j<sol_nplotlist;j++)
    {
      draw_cursor(sol_dipindex[sol_plotlist[j]],TRUE);
    }
} /*end draw_all_cursor*/

void
draw_all_vertex_cursor(void)
{
  int j;
  
  for (j=0;j<vertex_nplotlist;j++)
    {
      draw_cursor(vertex_plotlist[j],TRUE);
    }
} /*end draw_all_vertex_cursor*/

void
clear_all_vertex_cursor(void)
{
  int j;
  
  for (j=0;j<vertex_nplotlist;j++)
    {
      draw_cursor(vertex_plotlist[j],FALSE);
    }
} /*end clear_all_vertex_cursor*/


void
invert_vertex(int vno)
{
  VERTEX *v ;
  int    n ;
  FACE   *f ;
  
  v = &mris->vertices[vno] ;
  v->nx *= -1.0f ; v->ny *= -1.0f ; v->nz *= -1.0f ;
  for (n = 0 ; n < v->num ; n++)
    {
      f = &mris->faces[v->f[n]] ;
      f->nx *= -1.0f ; f->ny *= -1.0f ; f->nz *= -1.0f ;
    }
}

void
invert_face(int fno)
{
  FACE   *f ;
  
  if (fno < 0 || fno >= mris->nfaces)
    return ;
  f = &mris->faces[fno] ;
  f->nx *= -1.0f ; f->ny *= -1.0f ; f->nz *= -1.0f ;
}

void
orient_sphere(void)
{
  int    vno, n ;
  VERTEX *v ;
  
  mris->status = MRIS_SPHERE ;
  MRIScomputeMetricProperties(mris) ;
  for (vno = 0 ; vno < mris->nvertices ; vno++)
    {
      v = &mris->vertices[vno] ;
      v->stat = v->curv ;
      v->curv = 0.0 ;
    }
  for (vno = 0 ; vno < mris->nvertices ; vno++)
    {
      v = &mris->vertices[vno] ;
      for (n = 0 ; n < v->num ; n++)
  if (mris->faces[v->f[n]].area < 0)
    v->curv = -1 ;
    }
}

void
dump_faces(int vno)
{
  VERTEX *v ;
  int    n ;
  FACE   *f ;
  
  v = &mris->vertices[vno] ;
  for (n = 0 ; n < v->num ; n++)
    {
      f = &mris->faces[v->f[n]] ;
      fprintf(stderr, 
        "face %6d [%d,%d,%d], n = (%2.1f,%2.1f,%2.1f), area = %2.1f\n",
        v->f[n], f->v[0],f->v[1],f->v[2],f->nx, f->ny, f->nz, f->area) ;
    }
}

void
dump_vertex(int vno)
{
  VERTEX *v ;
  
  v = &mris->vertices[vno] ;
  fprintf(stderr, "v %d, n = (%2.2f, %2.2f, %2.2f), area = %2.2f\n",
    vno, v->nx, v->ny, v->nz, v->area) ;
  print_vertex_data(vno, stdout, 0.0) ;
}

/* Screencoord sx,sy; */
void
select_vertex(short sx,short sy)
{
  
  /* begin rkt */
  int vno;
  float d;
  VERTEX* v;
  
  /* sets dmin to the distance of the vertex found */
  find_vertex_at_screen_point(sx, sy, &vno, &d);
  if (vno>=0)
    {
      selection = vno;
      print_vertex_data(selection, stdout, d) ;
    }
  
  /* if we have functional data... */
  if(func_timecourse!=NULL)
    {
      v = &(mris->vertices[selection]);
      
      /* select only this voxel and graph it */
      func_clear_selection();
      func_select_ras(v->origx,v->origy,v->origz);
      func_graph_timecourse_selection();
    }
  
  /* select the label at this vertex, if there is one. */
  labl_select_label_by_vno (vno);
  
  /* select the boundary at this vertex, if there is one. */
  fbnd_select_boundary_by_vno (vno);
  
  /* finally, update the labels. */
  if (vno>=0)
    {
      update_labels(LABELSET_CURSOR, vno, d);
    }

  /* end rkt */
}

/* begin rkt */
void
find_vertex_at_screen_point(short sx,short sy,int* ovno, float* od) {
  
  int   i,j,imin = -1;
  float dx,dy,d,dmax,dmin,zmin=1000,zwin,cx,cy,cz;
  float nz,nzs,f;
  long ox,oy,lx,ly;
  float m[4][4]; /* Matrix m; */
  float wx,wy; /* Coord wx,wy; */
  VERTEX *v;
  
  nzs = 0.0f ;
  dmax = 2;
  dmin = 10;
  zwin = 5*zf;
  
  getmatrix(m);
#ifdef OPENGL
  for (i=0;i<4;i++)
    for (j=0;j<4;j++) {
      f = m[i][j];
      m[i][j] = m[j][i];
      m[j][i] = f;
    }
#endif
  getorigin(&ox,&oy);
  getsize(&lx,&ly);
  wx = (sx-ox-lx/2.0)*2*fov*sf/lx;
  wy = (sy-oy-ly/2.0)*2*fov*sf/ly;
  /*
    printf("sx=%d, sy=%d, ox=%d, oy=%d, lx=%d, ly=%d\n",
    sx,sy,ox,oy,lx,ly);
    printf("wx=%f, wy=%f\n",wx,wy);
    for (i=0;i<4;i++)
    for (j=0;j<4;j++)
    {
    printf("%f ",m[i][j]);
    if (j==3) printf("\n");
    }
  */
  for (i=0;i<mris->nvertices;i++)
    if (!mris->vertices[i].ripflag)
      {
  v = &mris->vertices[i];
  cx = -m[0][0]*v->x+m[1][0]*v->z+m[2][0]*v->y+m[3][0];
  cy = -m[0][1]*v->x+m[1][1]*v->z+m[2][1]*v->y+m[3][1];
  cz = -(-m[0][2]*v->x+m[1][2]*v->z+m[2][2]*v->y+m[3][2]);
  nz = -(-m[0][2]*v->nx+m[1][2]*v->nz+m[2][2]*v->ny);
  dx = cx-wx;
  dy = cy-wy;
  d = sqrt(dx*dx+dy*dy);
  d /= zf;
#if 0
  if (nz<0&&d<dmax/*&&v->d==0*/)   /* why was v->d==0 here????? (BRF) */
#else
    if (nz<0&&d<dmax&&v->d==0) 
#endif
      {
        if (cz<zmin-zwin||(cz<zmin+zwin&&d<dmin))
    {
      dmin = d;
      zmin = cz;
      imin = i;
      nzs = nz;
    }
      }
      }
  *ovno = imin;
  *od = dmin;
}
/* end rkt */

void
left_click(short sx,short sy)
{
#ifdef OPENGL
  sx += w.x;
  sy = 1024 - w.y - sy;
#endif
  
  if (selection>=0)
    draw_cursor(selection,FALSE);
  select_vertex(sx,sy);
  if (selection>=0)
    mark_vertex(selection,TRUE);
  if (selection>=0)
    draw_cursor(selection,TRUE);
}

void
sample_annotated_image(void)
{
  int i,j;
  int sx,sy;
  int c1,c2,c3;
  float cx,cy,cz,f;
  long ox,oy,lx,ly;
  float m[4][4]; /* Matrix m; */
  VERTEX *v;
  
  getmatrix(m);
#ifdef OPENGL
  for (i=0;i<4;i++)
    for (j=0;j<4;j++) {
      f = m[i][j];
      m[i][j] = m[j][i];
      m[j][i] = f;
    }
#endif
  getorigin(&ox,&oy);
  getsize(&lx,&ly);
  for (i=0;i<mris->nvertices;i++)
    if (!mris->vertices[i].ripflag)
      {
  v = &mris->vertices[i];
  cx = -m[0][0]*v->x+m[1][0]*v->z+m[2][0]*v->y+m[3][0];
  cy = -m[0][1]*v->x+m[1][1]*v->z+m[2][1]*v->y+m[3][1];
  cz = -(-m[0][2]*v->x+m[1][2]*v->z+m[2][2]*v->y+m[3][2]);
  sx = cx*lx/(2*fov*sf)+lx/2.0;
  sy = cy*ly/(2*fov*sf)+ly/2.0;
  /*
    printf("%d: x=%f y=%f z=%f, sx=%d, sy=%d\n",i,v->x,v->y,v->z,sx,sy);
  */
  if (sy>=0 && sy<frame_ydim && sx>=0 && sx<frame_xdim)
    {
      c1 = framebuff[sy*frame_xdim+sx]&0xff;
      c2 = (framebuff[sy*frame_xdim+sx]>>8)&0xff;
      c3 = (framebuff[sy*frame_xdim+sx]>>16)&0xff;
      if (c1!=0 && c1==c2 && c2==c3)
        {
    v->annotation = framebuff[sy*frame_xdim+sx]&0xff;
    /*
      printf("mris->vertices[%d].annotation=%06x\n",i,v->annotation);
    */
        }
    }
      }
}

void
restore_zero_position(void)
{
  if (!openglwindowflag) {
    printf("surfer: ### restore_zero_position failed: no gl window open\n");PR;
    return; 
  }
  
  loadmatrix(idmat);
  zf = 1.0;
}

void
restore_initial_position(void)
{
  if (!openglwindowflag) {
    printf("surfer: ### restore_initial_position failed: no gl window open\n");
    PR;
    return;
  }
  
  loadmatrix(idmat);
  translate(mris->xctr,-mris->zctr,-mris->yctr);
  zf = 1.0;
}

void
make_lateral_view(char *stem)
{
  if (!openglwindowflag) {
    printf("surfer: ### redraw failed: no gl window open\n");PR
                     return; }
  
  loadmatrix(idmat);
  translate(mris->xctr,-mris->zctr,-mris->yctr);
  if (stem[0]=='r'&&stem[1]=='h')
    rotate_brain(-90.0,'y');
  else
    rotate_brain(90.0,'y');
  zf = 1.0;
}

void
make_lateral_view_second(char *stem)
{
  if (!openglwindowflag) {
    printf(
     "surfer: ### make_lateral_view_second failed: no gl window open\n");PR
                     return; }
  if (!secondsurfaceloaded) {
    printf(
     "surfer: ### make_lateral_view_second failed: no second surface read\n");PR
                          return; }
  
  loadmatrix(idmat);
  translate(mris2->xctr,-mris2->zctr,-mris2->yctr);
  if (stem[0]=='r'&&stem[1]=='h')
    rotate_brain(-90.0,'y');
  else
    rotate_brain(90.0,'y');
  zf = 1.0;
}

void
write_val_histogram(float min, float max, int nbins)
{
  int i,num,index;
  FILE *fp;
  char fname[200]; 
  float sum,hist[10000],chist[10000];
  
  for (i=0;i<nbins;i++)
    hist[i] = chist[i] = 0;
  num = 0;
  for (i=0;i<mris->nvertices;i++)
    if (!mris->vertices[i].ripflag)
      {
  index = floor(nbins*(mris->vertices[i].val-min)/(max-min)+0.5);
  if (index>=0 && index<nbins)
    hist[index]++;
  /*
    if (index<0) index = 0;
    if (index>nbins-1) index = nbins-1;
  */
  num++;
      }
  sum = 0;
  for (i=0;i<nbins;i++)
    {
      hist[i] /= num;
      sum += hist[i];
      chist[i] = sum;
    }
  sprintf(fname,"hist.tmp");
  fp = fopen(fname,"w");
  if (fp==NULL) {printf("surfer: ### can't create file %s\n",fname);PR return;}
  for (i=0;i<nbins;i++)
    fprintf(fp,"%f %f\n",min+(i+0.5)*(max-min)/nbins,hist[i]);
  fclose(fp);
  sprintf(fname,"chist.tmp");
  fp = fopen(fname,"w");
  if (fp==NULL) {printf("surfer: ### can't create file %s\n",fname);PR return;}
  for (i=0;i<nbins;i++)
    fprintf(fp,"%f %f\n",min+(i+0.5)*(max-min)/nbins,chist[i]);
  fclose(fp);
}

void
write_view_matrix(char *dir)
{
  int i,j;
  float m[4][4]; /* Matrix m; */
  char fname[NAME_LENGTH];
  FILE *fp;
  
  if (!openglwindowflag) {
    printf("surfer: ### write_view_matrix failed: no gl window open\n");PR
                    return; }
  
  sprintf(fname,"%s/surfer.mat",dir);
  fp = fopen(fname,"w");
  if (fp==NULL) {printf("surfer: ### can't create file %s\n",fname);PR return;}
  getmatrix(m);
  for (i=0;i<4;i++) {
    for (j=0;j<4;j++) {
      fprintf(fp,"%13.3e ",m[i][j]);
    }
    fprintf(fp,"\n");
  }
  fclose(fp);
  printf("surfer: file %s written\n",fname);PR
                }

void
read_view_matrix(char *dir)
{
  int i;
  float m[4][4]; /* Matrix m; */
  float a,b,c,d;
  char line[NAME_LENGTH];
  char fname[NAME_LENGTH];
  FILE *fp;
  
  if (!openglwindowflag) {
    printf("surfer: ### read_view_matrix failed: no gl window open\n");PR
                   return; }
  
  sprintf(fname,"%s/surfer.mat",dir);
  fp = fopen(fname,"r");
  if (fp==NULL) {printf("surfer: ### File %s not found\n",fname);PR return;}
  
  i = 0;
  while (fgets(line,NAME_LENGTH,fp) != NULL) {
    if (sscanf(line,"%f %f %f %f",&a,&b,&c,&d) == 4)  {
      m[i][0] = a;
      m[i][1] = b;
      m[i][2] = c;
      m[i][3] = d;
      i++;
    }
    else {
      printf("surfer: ### couldn't parse this line in matrix file:  %s",line);
      printf("surfer: ###   ...read_view_matrix() failed\n");PR return;
    }
  }
  loadmatrix(m);
}

void
translate_brain(float x, float y, float z)
{
  int i,j,k;
  float m[4][4], m1[4][4], m2[4][4]; /* Matrix m,m1,m2; */
  
  if (!openglwindowflag) {
    printf("surfer: ### translate_brain failed: no gl window open\n");PR
                  return; }
  
  getmatrix(m);
  for (i=0;i<4;i++)
    for (j=0;j<4;j++)
      m1[i][j] = (i==j)?1.0:0.0;  
  m1[3][0] = x;
  m1[3][1] = y;
  m1[3][2] = z;
  for (i=0;i<4;i++)
    for (j=0;j<4;j++) {
      m2[i][j] = 0;
      for (k=0;k<4;k++)
  m2[i][j] += m[i][k]*m1[k][j];
    }
  loadmatrix(m2);
}

void
scale_brain(float s)
{
  int i,j,k;
  float m[4][4], m1[4][4], m2[4][4]; /* Matrix m,m1,m2; */
  
  if (!openglwindowflag) {
    printf("surfer: ### scale_brain failed: no gl window open\n");PR
                    return; }
  
  zf *= s;
  
  getmatrix(m);
  for (i=0;i<4;i++)
    for (j=0;j<4;j++)
      m1[i][j] = (i==j)?1.0:0.0;
  m1[0][0] = s;
  m1[1][1] = s;
  m1[2][2] = s;
  for (i=0;i<4;i++)
    for (j=0;j<4;j++) {
      m2[i][j] = 0;
      for (k=0;k<4;k++)
  m2[i][j] += m[i][k]*m1[k][j];
    }
  loadmatrix(m2);
}

void
rotate_brain(float a, char c)
{
  int i,j,k;
  float m[4][4], m1[4][4], m2[4][4]; /* Matrix m,m1,m2; */
  float sa,ca;
  
  if (!openglwindowflag) {
    printf("surfer: ### rotate_brain failed: no gl window open\n");PR
                     return; }
  
  getmatrix(m);
  for (i=0;i<4;i++)
    for (j=0;j<4;j++)
      m1[i][j] = (i==j)?1.0:0.0;
  a = a*M_PI/180;
  sa = sin(a);
  ca = cos(a);
  if (c=='y') {
    m1[0][0] = m1[2][2] = ca;
    m1[2][0] = -(m1[0][2] = sa);
  } 
  else if (c=='x') {
    m1[1][1] = m1[2][2] = ca;
    m1[1][2] = -(m1[2][1] = sa);
  } 
  else if (c=='z') {
    m1[0][0] = m1[1][1] = ca;
    m1[1][0] = -(m1[0][1] = sa);
  } 
  else {
    printf("surfer: ### Illegal axis %c\n",c);return;PR
                   }
  for (i=0;i<4;i++)
    for (j=0;j<4;j++) {
      m2[i][j] = 0;
      for (k=0;k<4;k++)
  m2[i][j] += m[i][k]*m1[k][j];
    }
  loadmatrix(m2);
}

void
read_image_info(char *fpref)
{
  FILE *fptr;
  char fname[NAME_LENGTH], char_buf[100];
  
  sprintf(fname,"%s.info",fpref);
  /*  printf("surfer: read_image_info:\n");*/
  printf("surfer: %s\n",fname);
  fptr = fopen(fname,"r");
  if (fptr==NULL) {printf("surfer: ### File %s not found.\n",fname);exit(1);}
  fscanf(fptr,"%*s %d",&imnr0);
  fscanf(fptr,"%*s %d",&imnr1);
  fscanf(fptr,"%*s %*d");
  fscanf(fptr,"%*s %d",&xnum);
  fscanf(fptr,"%*s %d",&ynum);
  fscanf(fptr,"%*s %f",&fov);
  fscanf(fptr,"%*s %f",&ps);
  fscanf(fptr,"%*s %f",&st);
  fscanf(fptr,"%*s %*f"); /* locatn */
  fscanf(fptr,"%*s %f",&xx0); /* strtx */
  fscanf(fptr,"%*s %f",&xx1); /* endx */
  fscanf(fptr,"%*s %f",&yy0); /* strty */
  fscanf(fptr,"%*s %f",&yy1); /* endy */
  fscanf(fptr,"%*s %f",&zz0); /* strtz */
  fscanf(fptr,"%*s %f",&zz1); /* endz */
  fscanf(fptr, "%*s %*f") ;   /* tr */
  fscanf(fptr, "%*s %*f") ;   /* te */
  fscanf(fptr, "%*s %*f") ;   /* ti */
  fscanf(fptr, "%*s %s",char_buf);
  fclose(fptr);
  
  fov *= 1000;
  ps *= 1000;
  st *= 1000;
  xx0 *= 1000;
  xx1 *= 1000;
  yy0 *= 1000;
  yy1 *= 1000;
  zz0 *= 1000;
  zz1 *= 1000;
  numimg = imnr1-imnr0+1;
}

void
read_talairach(char *fname)    /* marty: ignore abs paths in COR-.info */
{
  lta = LTAread(fname) ;
  if (lta==NULL)
    printf("surfer: Talairach xform file not found (ignored)\n");
  else 
    transform_loaded = TRUE;
}

void
read_images(char *fpref)
{
  int i,k;
  FILE *fptr;
  char fname[NAME_LENGTH];
  
  numimg = imnr1-imnr0+1;
  bufsize = ((unsigned long)xnum)*ynum;
  if (buf==NULL) buf = (unsigned char *)lcalloc(bufsize,sizeof(char));
  for (k=0;k<numimg;k++)
    {
      im[k] = (unsigned char **)lcalloc(IMGSIZE,sizeof(char *));
      for (i=0;i<IMGSIZE;i++)
  {
    im[k][i] = (unsigned char *)lcalloc(IMGSIZE,sizeof(char));
  }
    }
  printf("surfer: allocated image buffer (16 Meg)\n");PR
              
              for (k=0;k<numimg;k++)
                {
                  file_name(fpref,fname,k+imnr0,"%03d");
                  fptr = fopen(fname,"r");
                  if (fptr==NULL) {
                    printf("surfer: ### File %s not found\n",fname);exit(1);}
                  fread(buf,sizeof(char),bufsize,fptr);
                  buffer_to_image(buf,im[k],xnum,ynum);
                  fclose(fptr);
                  printf("read image %d (%s)\n",k,fname);
                }
}

void
alloc_curv_images(void)
{
  int i,k;
  
  for (k=0;k<numimg;k++) {
    curvim[k] = (unsigned char **)lcalloc(IMGSIZE,sizeof(char *));
    ocurvim[k] = (unsigned char **)lcalloc(IMGSIZE,sizeof(char *));
    curvimflags[k] = (unsigned char **)lcalloc(IMGSIZE,sizeof(char *));
    for (i=0;i<IMGSIZE;i++) {
      curvim[k][i] = (unsigned char *)lcalloc(IMGSIZE,sizeof(char));
      ocurvim[k][i] = (unsigned char *)lcalloc(IMGSIZE,sizeof(char));
      curvimflags[k][i] = (unsigned char *)lcalloc(IMGSIZE,sizeof(char));
    }
  }
  curvim_allocated = TRUE;
  printf("surfer: allocated curv,ocurv,flags images (48 Meg)\n");PR
                   }

void
read_curv_images(char *fpref)/* assumes norm'ed curvim:{CURVIM_NORM_MIN,MAX} */
{
  int i,j,k;
  char fname[NAME_LENGTH];
  FILE *fptr;
  
  if (!curvim_allocated) alloc_curv_images();
  
  for (k=0;k<numimg;k++)
    {
      file_name(fpref,fname,k+imnr0,"%03d");
      fptr = fopen(fname,"r");
      if (fptr==NULL) {
  printf("surfer: ### File %s not found\n",fname);PR return;}
      if (buf==NULL) 
  buf = (unsigned char *)lcalloc(IMGSIZE*IMGSIZE,sizeof(char));
      fread(buf,sizeof(char),IMGSIZE*IMGSIZE,fptr);
      buffer_to_image(buf,curvim[k],xnum,ynum);
      fclose(fptr);
      printf("surfer: read curv image %d (%s)\n",k,fname);
    }
  /* next two used by:  ellipsoid_shrink, second_surface_curv_to_curvim */
  /*   smooth_curvim_sparse, read_curvim_at_vertex, curv_shrink_to_fill */
  mris2->min_curv = CURVIM_NORM_MIN;
  mris2->max_curv = CURVIM_NORM_MAX;
  
  /* hack: byte0 => UNDEFINED; should save flags to read w/images!! */
  /* no FIXEDVAL allows smooth; should write smooth_curvim() */
  for(k=0;k<numimg;k++)
    for(i=0;i<IMGSIZE;i++)
      for(j=0;j<IMGSIZE;j++) {
  if (curvim[k][i][j]!=0)
    curvimflags[k][i][j] |= CURVIM_DEFINED;
      }
  printf("surfer: non-zero bytes DEFINED (not FIXEDVAL) in curv image\n");
  
  curvimflag = TRUE;
  curvimloaded = TRUE;
}

void
curv_to_curvim(void)
{
  VERTEX *v;
  int imnr,i,j,k,vdef,pdef;
  float x,y,z;
  
  if (!curvim_allocated) {
    alloc_curv_images();
  }
  else {
    for(k=0;k<numimg;k++)
      for(i=0;i<IMGSIZE;i++)
  for(j=0;j<IMGSIZE;j++)
    curvim[k][i][j] = ocurvim[k][i][j] = curvimflags[k][i][j] = 0;
    printf("surfer: cleared curv,ocurv,flags images");PR
              }
  
  vdef = pdef = 0;
  for (k=0;k<mris->nvertices;k++) {
    v = &mris->vertices[k];
    x = v->x;
    y = v->y;
    z = v->z;
    imnr = (int)((y-yy0)/st+0.5);
    i = (int)((zz1-z)/ps+0.5);
    j = (int)((xx1-x)/ps+0.5);
    imnr = (imnr<0)?0:(imnr>=numimg)?numimg-1:imnr;
    i = (i<0)?0:(i>=IMGSIZE)?IMGSIZE-1:i;
    j = (j<0)?0:(j>=IMGSIZE)?IMGSIZE-1:j;
    curvim[imnr][i][j] = floattobyte(v->curv,CURVIM_NORM_MIN,CURVIM_NORM_MAX);
    if (!(curvimflags[imnr][i][j] & CURVIM_DEFINED))
      pdef++;
    curvimflags[imnr][i][j] = CURVIM_DEFINED | CURVIM_FIXEDVAL;
    vdef++;
  }
  printf("surfer: curv image made--%d pix set\n",vdef);PR
               printf("surfer:                  %d unique pix\n",pdef);PR
                               curvimflag = TRUE;
  curvimloaded = TRUE;
}

void
second_surface_curv_to_curvim(void)
{
  VERTEX *v;
  int imnr,i,j,k,vdef,pdef;
  float x,y,z;
  
  if (!secondsurfaceloaded) {
    printf("surfer: ### second surface not loaded!\n");PR return; }
  if (!secondcurvloaded) {
    printf("surfer: ### second curv not loaded!\n");PR return; }
  
  if (!curvim_allocated) {
    alloc_curv_images();
  } 
  else {
    for(k=0;k<numimg;k++)
      for(i=0;i<IMGSIZE;i++)
  for(j=0;j<IMGSIZE;j++)
    curvim[k][i][j] = ocurvim[k][i][j] = curvimflags[k][i][j] = 0;
    printf("surfer: cleared curv,ocurv,flags images");PR
              }
  
  vdef = pdef = 0;
  for (k=0;k<mris2->nvertices;k++) {
    v = &mris2->vertices[k];
    x = v->x;
    y = v->y;
    z = v->z;
    imnr = (int)((y-yy0)/st+0.5);
    i = (int)((zz1-z)/ps+0.5);
    j = (int)((xx1-x)/ps+0.5);
    imnr = (imnr<0)?0:(imnr>=numimg)?numimg-1:imnr;
    i = (i<0)?0:(i>=IMGSIZE)?IMGSIZE-1:i;
    j = (j<0)?0:(j>=IMGSIZE)?IMGSIZE-1:j;
    curvim[imnr][i][j] = floattobyte(v->curv,mris2->min_curv,mris2->max_curv);
    if (!(curvimflags[imnr][i][j] & CURVIM_DEFINED))
      pdef++;
    curvimflags[imnr][i][j] = CURVIM_DEFINED | CURVIM_FIXEDVAL;
    vdef++;
  }
  printf("surfer: curv image made from second surface--%d pix set\n",vdef);PR
                       printf("surfer:                                      %d unique pix\n",pdef);PR
                                         curvimflag = TRUE;
  curvimloaded = TRUE;
}

void
swap_curv(void)
{
  VERTEX *v;
  int k;
  float tmp;
  
  for (k=0;k<mris->nvertices;k++)
    {
      v = &mris->vertices[k];
      tmp = v->curv;
      v->curv = v->curvbak;
      v->curvbak = tmp;
    }
}

/* begin rkt */
void
swap_vertex_fields(int typea, int typeb) 
{
  VERTEX *v;
  int k;
  float *a;
  float tmp;
  
  enable_menu_set (MENUSET_OVERLAY_LOADED, 1);
  
  /* for every vertex, swap the values specified in the parameters */
  for (k=0;k<mris->nvertices;k++)
    {
      v = &mris->vertices[k];
      /* point us in the direction of the first field */
      a = NULL;
      switch(typea)
  {
  case FIELD_CURV:
    a = &(v->curv);
    break;
  case FIELD_CURVBAK:
    a = &(v->curvbak);
    break;
  case FIELD_VAL:
    a = &(v->val);
    break;
  case FIELD_IMAG_VAL:
    a = &(v->imag_val);
    break;
  case FIELD_VAL2:
    a = &(v->val2);
    break;
  case FIELD_VALBAK:
    a = &(v->valbak);
    break;
  case FIELD_VAL2BAK:
    a = &(v->val2bak);
    break;
  case FIELD_STAT:
    a = &(v->stat);
    break;
  default:
    printf("### surfer: can't switch surface fields, invalid type %d\n",
     typea);
  }
      /* save the value */
      tmp = *a;
      /* get the second field. set the value of the first field to the second
   field and set the second to the saved value. */
      switch(typeb)
  {
  case FIELD_CURV:
    *a = v->curv;
    v->curv = tmp;
    break;
  case FIELD_CURVBAK:
    *a = v->curvbak;
    v->curvbak = tmp;
    break;
  case FIELD_VAL:
    *a = v->val;
    v->val = tmp;
    break;
  case FIELD_IMAG_VAL:
    *a = v->imag_val;
    v->imag_val = tmp;
    break;
  case FIELD_VAL2:
    *a = v->val2;
    v->val2 = tmp;
    break;
  case FIELD_VALBAK:
    *a = v->valbak;
    v->valbak = tmp;
    break;
  case FIELD_VAL2BAK:
    *a = v->val2bak;
    v->val2bak = tmp;
    break;
  case FIELD_STAT:
    *a = v->stat;
    v->stat = tmp;
    break;
  default:
    printf("### surfer: can't switch surface fields, invalid type %d\n",
     typeb);
  }
    }
  
  /* TODO: update_labels for last selected vertex */
}
/* end rkt */

void
curvim_to_surface(void)   /* assumes norm'ed curvim:{CURVIM_NORM_MIN,MAX} */
{
  VERTEX *v;
  int imnr,i,j,k;
  float x,y,z;
  
  if (!curvimloaded) { printf("surfer: ### curvim not loaded!\n");PR return; }
  
  for (k=0;k<mris->nvertices;k++) {
    v = &mris->vertices[k];
    x = v->x;
    y = v->y;
    z = v->z;
    imnr = (int)((y-yy0)/st+0.5);
    i = (int)((zz1-z)/ps+0.5);
    j = (int)((xx1-x)/ps+0.5);
    imnr = (imnr<0)?0:(imnr>=numimg)?numimg-1:imnr;
    i = (i<0)?0:(i>=IMGSIZE)?IMGSIZE-1:i;
    j = (j<0)?0:(j>=IMGSIZE)?IMGSIZE-1:j;
    
    /* hack: byte 0 => UNDEFINED; should save flags w/images!! */
    /* no FIXEDVAL allows smooth; should write smooth_curvim() */
    if (curvim[imnr][i][j]==0) {
      v->curv = 0.0;
    }
    else {
      v->curv= bytetofloat(curvim[imnr][i][j],CURVIM_NORM_MIN,CURVIM_NORM_MAX);
      curvimflags[imnr][i][j] |= CURVIM_DEFINED; /* Why? AMD */
    }
  }
}

/* assumes norm'ed curvim:{CURVIM_NORM_MIN,MAX} */
void
curvim_to_second_surface(void)
{
  VERTEX *v;
  int imnr,i,j,k;
  float x,y,z;
  
  if (!curvimloaded) { printf("surfer: ### curvim not loaded!\n");PR return; }
  if (!secondsurfaceloaded) {
    printf("surfer: ### second surface not loaded!\n");PR return; }
  
  for (k=0;k<mris2->nvertices;k++) {
    v = &mris2->vertices[k];
    x = v->x;
    y = v->y;
    z = v->z;
    imnr = (int)((y-yy0)/st+0.5);
    i = (int)((zz1-z)/ps+0.5);
    j = (int)((xx1-x)/ps+0.5);
    imnr = (imnr<0)?0:(imnr>=numimg)?numimg-1:imnr;
    i = (i<0)?0:(i>=IMGSIZE)?IMGSIZE-1:i;
    j = (j<0)?0:(j>=IMGSIZE)?IMGSIZE-1:j;
    
    /* hack: byte 0 => UNDEFINED; should save flags w/images!! */
    /* no FIXEDVAL allows smooth; should write smooth_curvim() */
    if (curvim[imnr][i][j]==0) {
      v->curv = 0.0;
    }
    else {
      v->curv = bytetofloat(curvim[imnr][i][j],CURVIM_NORM_MIN,CURVIM_NORM_MAX);
      curvimflags[imnr][i][j] |= CURVIM_DEFINED;
    }
  }
  secondcurvloaded = TRUE;
}

void
smooth_curvim(int window)
{
  int i,j,k,di,dj,dk;
  double avgcurv,numcurv;
  
  printf("smooth_curvim(%d)\n",window);
  
  for (k=0;k<numimg;k++)
    for(i=0;i<IMGSIZE;i++)
      for(j=0;j<IMGSIZE;j++)
  ocurvim[k][i][j] = curvim[k][i][j];
  
  for(i=0;i<IMGSIZE;i++)
    {
      printf(".");fflush(stdout);
      for(j=0;j<IMGSIZE;j++)
  {
    for (k=0;k<numimg;k++)
      if (curvimflags[k][i][j] & CURVIM_DEFINED)
        {
    avgcurv = numcurv = 0;
    for (dk= -window;dk<=window;dk++)
      if ((k+dk>=0)&&(k+dk<numimg))
        if (curvimflags[k+dk][i][j] & CURVIM_DEFINED)
          {
      avgcurv += bytetofloat(ocurvim[k+dk][i][j],CURVIM_NORM_MIN,CURVIM_NORM_MAX);
      numcurv++;
          }
    if (numcurv>0)
      avgcurv /= numcurv;
    curvim[k][i][j] = floattobyte(avgcurv,CURVIM_NORM_MIN,CURVIM_NORM_MAX);
        }
  }
    }
  printf("\n");
  
  for (k=0;k<numimg;k++)
    for(i=0;i<IMGSIZE;i++)
      for(j=0;j<IMGSIZE;j++)
  ocurvim[k][i][j] = curvim[k][i][j];
  
  for(k=0;k<numimg;k++)
    {
      printf(".");fflush(stdout);
      for(j=0;j<IMGSIZE;j++)
  {
    for (i=0;i<IMGSIZE;i++)
      if (curvimflags[k][i][j] & CURVIM_DEFINED)
        {
    avgcurv = numcurv = 0;
    for (di= -window;di<=window;di++)
      if ((i+di>=0)&&(i+di<IMGSIZE))
        if (curvimflags[k][i+di][j] & CURVIM_DEFINED)
          {
      avgcurv += bytetofloat(ocurvim[k][i+di][j],CURVIM_NORM_MIN,CURVIM_NORM_MAX);
      numcurv++;
          }
    if (numcurv>0)
      avgcurv /= numcurv;
    curvim[k][i][j] = floattobyte(avgcurv,CURVIM_NORM_MIN,CURVIM_NORM_MAX);
        }
  }
    }
  printf("\n");
  
  for (k=0;k<numimg;k++)
    for(i=0;i<IMGSIZE;i++)
      for(j=0;j<IMGSIZE;j++)
  ocurvim[k][i][j] = curvim[k][i][j];
  
  for(k=0;k<numimg;k++)
    {
      printf(".");fflush(stdout);
      for(i=0;i<IMGSIZE;i++)
  {
    for (j=0;j<IMGSIZE;j++)
      if (curvimflags[k][i][j] & CURVIM_DEFINED)
        {
    avgcurv = numcurv = 0;
    for (dj= -window;dj<=window;dj++)
      if ((j+dj>=0)&&(j+dj<IMGSIZE))
        if (curvimflags[k][i][j+dj] & CURVIM_DEFINED)
          {
      avgcurv += bytetofloat(ocurvim[k][i][j+dj],CURVIM_NORM_MIN,CURVIM_NORM_MAX);
      numcurv++;
          }
    if (numcurv>0)
      avgcurv /= numcurv;
    curvim[k][i][j] = floattobyte(avgcurv,CURVIM_NORM_MIN,CURVIM_NORM_MAX);
        }
  }
    }
  printf("\n");
}

#if 0
smooth_curvim(window)
     int window;
{
  int i,j,k,di,dj,dk;
  double curv,avgcurv,numcurv;
  
  printf("smooth_curvim(%d)\n",window);
  for (k=0;k<numimg;k++)
    for(i=0;i<IMGSIZE;i++)
      for(j=0;j<IMGSIZE;j++)
  ocurvim[k][i][j] = curvim[k][i][j];
  
  for (k=0;k<numimg;k++)
    {
      printf(".");fflush(stdout);
      for(i=0;i<IMGSIZE;i++)
  for(j=0;j<IMGSIZE;j++)
    if (curvimflags[k][i][j] & CURVIM_DEFINED)
      {
        avgcurv = numcurv = 0;
        for (dk= -window;dk<=window;dk++)
    for (di= -window;di<=window;di++)
      for (dj= -window;dj<=window;dj++)
        if ((k+dk>=0)&&(k+dk<numimg)&&
      (i+di>=0)&&(i+di<IMGSIZE)&&
      (j+dj>=0)&&(j+dj<IMGSIZE))
          if (curvimflags[k+dk][i+di][j+dj] & CURVIM_DEFINED)
      {
        avgcurv += bytetofloat(ocurvim[k+dk][i+di][j+dj],mris2->min_curv,mris2->max_curv);
        numcurv++;
      }
        avgcurv /= numcurv;
        curvim[k][i][j] = floattobyte(avgcurv,mris2->min_curv,mris2->max_curv);
      }
    }
  printf("\n");
}
#endif

/* assumes norm'd curvim */
void
add_subject_to_average_curvim(char *name, char *morphsubdir)  
{
  int i,j,k;
  float curv,avgcurv;
  char fname[NAME_LENGTH],fpref[NAME_LENGTH];
  
  /* check if images there first */
  /*sprintf(fpref,"%s/%s/%s.%s.%s/COR-",subjectsdir,name,CURVDIR_STEM,stem,ext);*/
  sprintf(fname,"%s/%s/morph/%s/COR-001",subjectsdir,name,morphsubdir);
  if (fopen(fname,"r")==NULL) {
    printf("surfer: ### File %s not found\n",fname);PR return;}
  
  /* avgcurv->old */
  if (!curvim_allocated)
    alloc_curv_images();
  else {
    for(k=0;k<numimg;k++)
      for(i=0;i<IMGSIZE;i++)
  for(j=0;j<IMGSIZE;j++) {
    ocurvim[k][i][j] = curvim[k][i][j];
  }
  }
  /*sprintf(fpref,"%s/%s/%s.%s.%s/COR-",subjectsdir,name,CURVDIR_STEM,stem,ext);*/
  sprintf(fpref,"%s/%s/morph/%s/COR-",subjectsdir,name,morphsubdir);
  read_curv_images(fpref);
  
  if (curvim_averaged) {
    for(k=0;k<numimg;k++)
      for(i=0;i<IMGSIZE;i++)
  for(j=0;j<IMGSIZE;j++) {
    avgcurv = bytetofloat(ocurvim[k][i][j],CURVIM_NORM_MIN,CURVIM_NORM_MAX);
    curv = bytetofloat(curvim[k][i][j],CURVIM_NORM_MIN,CURVIM_NORM_MAX);
    avgcurv = (avgcurv*curvim_averaged + curv)/(curvim_averaged+1);
    curvim[k][i][j] = floattobyte(avgcurv,CURVIM_NORM_MIN,CURVIM_NORM_MAX);
  }
  }
  else {
    curvimflag = TRUE;
    curvimloaded = TRUE;
  }
  curvim_averaged++;
  printf("surfer: %d subjects in average curvim\n",curvim_averaged);
}

void
smooth_curvim_sparse(int niter)
{
  int iter,i,j,k,n;
  int i2,j2,k2;
  int ndef;
  float sum;
  
  printf("surfer: smooth_curvim_sparse:\n");PR
                for (iter=0;iter<niter;iter++) {
            ndef = 0;
            for(k=0;k<numimg;k++)
              for(i=0;i<IMGSIZE;i++)
                for(j=0;j<IMGSIZE;j++) {
                  ocurvim[k][i][j] = curvim[k][i][j];
                  if (curvimflags[k][i][j] & CURVIM_DEFINED) {
              curvimflags[k][i][j] |= CURVIM_DEFINED_OLD;
              ndef++;
                  }
                }
            printf("surfer:  iter = %d  defined curv pix = %d\n",iter,ndef);PR
                              for(k=1;k<numimg-1;k++)
                                for(i=1;i<IMGSIZE-1;i++)
                                  for(j=1;j<IMGSIZE-1;j++) {
                              if (!(curvimflags[k][i][j] & CURVIM_FIXEDVAL)) {
                                sum = 0;
                                n = 0;
#if 0
                                if (curvimflags[k][i][j] & CURVIM_DEFINED_OLD) {  /* center + 6 */
                                  sum += bytetofloat(ocurvim[k2][i][j],mris2->min_curv,mris2->max_curv);
                                  n++;
                                }
                                for(k2=k-1;k2<k+2;k2+=2) {  /* 6 */
                                  if (curvimflags[k2][i][j] & CURVIM_DEFINED_OLD) {
                                    sum += bytetofloat(ocurvim[k2][i][j],mris2->min_curv,mris2->max_curv);
                                    n++;
                                  }
                                }
                                for(i2=i-1;i2<i+2;i2+=2) {
                                  if (curvimflags[k][i2][j] & CURVIM_DEFINED_OLD) {
                                    sum += bytetofloat(ocurvim[k][i2][j],mris2->min_curv,mris2->max_curv);
                                    n++;
                                  }
                                }
                                for(j2=j-1;j2<j+2;j2+=2) {
                                  if (curvimflags[k][i][j2] & CURVIM_DEFINED_OLD) {
                                    sum += bytetofloat(ocurvim[k][i][j2],mris2->min_curv,mris2->max_curv);
                                    n++;
                                  }
                                }
#endif
                                for(k2=k-1;k2<k+2;k2++)  /* 27 */
                                  for(i2=i-1;i2<i+2;i2++)
                                    for(j2=j-1;j2<j+2;j2++) {
                                if (curvimflags[k2][i2][j2] & CURVIM_DEFINED_OLD) {
                                  sum += bytetofloat(ocurvim[k2][i2][j2],mris2->min_curv,mris2->max_curv);
                                  n++;
                                }
                                    }
                                if (n>0) {
                                  curvim[k][i][j] = floattobyte(sum/n,mris2->min_curv,mris2->max_curv);
                                  curvimflags[k][i][j] |= CURVIM_DEFINED;
                                }
                              }
                                  }
                }
}

unsigned char 
floattobyte(float f, float min, float max)
{
  f = (f>max)?max:(f<min)?min:f;  /* needed? */
  return (unsigned char)((f-min)*(255/(max-min)));
}

float 
bytetofloat(unsigned char c, float min, float max)
{
  /*if ((int)c>255) {
    printf("surfer: bad input byte %d for current 255\n",(int)c);
    return HUGE_VAL;
    }*/
  return (float)c/(255/(max-min)) + min;
}

/* tksurfer.c: write_{curv,fill}_images */
void
write_images(unsigned char ***mat,char *fpref)
{
  int k;                   
  FILE *fptr;
  char fname[NAME_LENGTH];
  
  bufsize = ((unsigned long)xnum)*ynum;
  if (buf==NULL) buf = (unsigned char *)lcalloc(bufsize,sizeof(char));
  for (k=0;k<numimg;k++)
    {
      file_name(fpref,fname,k+1,"%03d");
      fptr = fopen(fname,"w");
      if (fptr==NULL) {
  printf("surfer: ### can't create file %s\n",fname);PR return;}
      image_to_buffer(mat[k],buf,xnum,ynum);
      fwrite(buf,sizeof(char),bufsize,fptr);
      fclose(fptr);
      printf("surfer: file %s written\n",fname);PR
              }
}

void
save_surf(void)
{
  MRISsaveVertexPositions(mris, TMP_VERTICES) ;
}
void
restore_surf(void)
{
  MRISrestoreVertexPositions(mris, TMP_VERTICES) ;
  MRIScomputeNormals(mris) ;
}
void
read_positions(char *name)
{
  if (MRISreadVertexPositions(mris, name) == NO_ERROR)
    {
      mris->status = MRIS_SURFACE ;
      MRIScomputeMetricProperties(mris) ;
    }
  surface_compiled = 0 ;
}
int
read_binary_surface(char *fname)
{
  if (mris)
    MRISfree(&mris) ;
  mris = MRISread(ifname) ;
  if (!mris)
    return(Gerror) ;
  MRISsaveVertexPositions(mris, TMP_VERTICES) ;
  flag2d = FALSE ;
  if (!mris)
    return(ERROR_NOFILE) ;
  MRISclearCurvature(mris) ;
  printf("surfer: vertices=%d, faces=%d\n",mris->nvertices,mris->nfaces);
  surface_compiled = 0 ;
  return(NO_ERROR) ;
}


void
read_second_binary_surface(char *fname)   /* inlined hack */
{
  mris2 = MRISread(fname) ;
  PR
    }

void
read_second_binary_curvature(char *fname)
{
  MRISreadCurvatureFile(mris2, fname) ;
  secondcurvloaded = TRUE;
  surface_compiled = 0 ;
  printf("surfer: second curvature read: min=%f max=%f\n",mris2->min_curv,mris2->max_curv);
}


void
normalize_second_binary_curvature(void)
{
  int k;
  float curv,min,max;
  float sum,avg,sum_sq,sd,n;
  
  min = max = 0.0f ;
  if (!secondcurvloaded) { 
    printf("surfer: ### second curv not loaded!\n");PR return; }
  
  sum = 0;
  for (k=0;k<mris2->nvertices;k++)
    sum += mris2->vertices[k].curv;
  avg = sum/mris2->nvertices;
  
  n = (float)mris2->nvertices;
  sum = sum_sq = 0.0;
  for (k=0;k<mris2->nvertices;k++) {
    mris2->vertices[k].curv -= avg;
    curv = mris2->vertices[k].curv;
    sum += curv;
    sum_sq += curv*curv;
  }
  sd = sqrt((n*sum_sq - sum*sum)/(n*(n-1.0)));
  
  for (k=0;k<mris2->nvertices;k++) {
    curv = (mris2->vertices[k].curv)/sd;
    if (k==0) min=max=curv;
    if (curv>max) max=curv;
    if (curv<min) min=curv;
    if (curv<CURVIM_NORM_MIN) curv = CURVIM_NORM_MIN;
    if (curv>CURVIM_NORM_MAX) curv = CURVIM_NORM_MAX;
    mris2->vertices[k].curv = curv;
  }
  mris2->min_curv = CURVIM_NORM_MIN;
  mris2->max_curv = CURVIM_NORM_MAX;
  printf("surfer: second curvature normalized: avg=%f sd=%f\n",avg,sd);
  printf("surfer: min=%f max=%f trunc (%f,%f)\n",min,max,mris2->min_curv,mris2->max_curv);PR
                          }

static void
read_imag_vals(char *fname)
{
  MRISreadImagValues(mris, fname) ;
}

static char *last_subject_name = NULL ;

static void
resend_to_subject(void)
{
  if (!last_subject_name)
    {
      printf("must send_to_subject to specify subject name first.\n") ;
      return ;
    }
  
  send_to_subject(last_subject_name) ;
}

void
send_to_subject(char *subject_name)
{
  char canon_name[STRLEN], orig_name[STRLEN] ;
  
  sprintf(canon_name, "%s.%s", stem, sphere_reg) ;
  sprintf(orig_name, "%s.white", stem) ;
#if 0
  send_spherical_point(subject_name, canon_name, orig_name) ;
#else
  send_spherical_point(subject_name, canon_name, 
                       FileNameOnly(orfname, orig_name)) ;
#endif
}
void
send_spherical_point(char *subject_name, char *canon_name, char *orig_name)
{
  float           x, y, z, dx, dy, dz, dist, min_dist ;
  SMALL_VERTEX    *sv ;
  VERTEX          *v ;
  char            fname[STRLEN] ;
  SMALL_SURFACE   *mriss ;
  int             vno, min_vno ;
  FILE            *fp ;
  
  if (selection < 0)
    {
      printf("must select a vertex.\n") ;
      return ;
    }
  if (canonsurfloaded == FALSE)
    {
      if (DIAG_VERBOSE_ON)
  printf("reading canonical vertex positions from %s...\n", canon_name) ;
      MRISsaveVertexPositions(mris, TMP_VERTICES) ;
      if (MRISreadVertexPositions(mris, canon_name) != NO_ERROR)
  {
    canonsurffailed = TRUE ;
    return ;
  }
      MRISsaveVertexPositions(mris, CANONICAL_VERTICES) ;
      MRISrestoreVertexPositions(mris, TMP_VERTICES) ;
      canonsurfloaded = TRUE ;
    }
  
  sprintf(fname, "%s/%s/surf/%s", subjectsdir, subject_name, canon_name) ;
  if (DIAG_VERBOSE_ON)
    printf("reading spherical coordinates for subject %s from %s...\n",
           subject_name, fname) ;
  
  mriss = MRISSread(fname) ;
  if (!mriss)
    {
      fprintf(stderr, "### could not open surface file %s\n", fname) ;
      return ;
    }
  
  v = &mris->vertices[selection] ;
  x = v->cx ; y = v->cy ; z = v->cz ; 
  min_dist = 1000000.0f ; min_vno = -1 ;
  for (vno = 0 ; vno < mriss->nvertices ; vno++)
    {
      sv = &mriss->vertices[vno] ;
      dx = sv->x - x ; dy = sv->y - y ; dz = sv->z - z ; 
      dist = sqrt(dx*dx + dy*dy + dz*dz) ;
      if (dist < min_dist)
  {
    min_dist = dist ;
    min_vno  = vno ;
  }
    }
  MRISSfree(&mriss) ;
  
  printf("closest vertex %d is %2.1f mm distant.\n", min_vno, min_dist) ;
  
  sprintf(fname, "%s/%s/surf/%s", subjectsdir, subject_name, orig_name) ;
  if (DIAG_VERBOSE_ON)
    printf("reading original coordinates for subject %s from %s...\n",
           subject_name, fname) ;
  
  mriss = MRISSread(fname) ;
  if (!mriss)
    {
      fprintf(stderr, "### could not open surface file %s\n", fname) ;
      return ;
    }
  
  sv = &mriss->vertices[min_vno] ;
  sprintf(fname,"%s/%s/%s/edit.dat",subjectsdir,subject_name,TMP_DIR);
  if (DIAG_VERBOSE_ON)
    printf("writing coordinates to file %s\n", fname) ;
  fp = fopen(fname,"w");
  if (fp==NULL) 
    {
      printf("surfer: ### can't create file %s\n",fname);
      PR return;
    }
  if (DIAG_VERBOSE_ON)
    printf("vertex %d coordinates:\n", min_vno);
#if 0
  if (transform_loaded) 
    printf("TALAIRACH (%2.1f %2.1f %2.1f)\n",x_tal,y_tal,z_tal);
#endif
  if (DIAG_VERBOSE_ON)
    printf("ORIGINAL  (%2.1f %2.1f %2.1f)\n",x,y,z); PR
                   fprintf(fp,"%f %f %f\n",sv->x,sv->y,sv->z);
#if 0
  fprintf(fp,"%f %f %f\n",x_tal,y_tal,z_tal);
#endif
  fclose(fp);
}
int
read_white_vertex_coordinates(void)
{
  fprintf(stderr, "reading white matter vertex locations...\n") ;
  if (MRISreadOriginalProperties(mris, "white") != NO_ERROR)
    return(Gerror) ;
  surface_compiled = 0 ;
  white_surf_loaded = 1 ;
  return(NO_ERROR) ;
}
int
read_inflated_vertex_coordinates(void)
{
  fprintf(stderr, "reading inflated vertex locations...\n") ;
  if (MRISreadInflatedCoordinates(mris, "inflated") != NO_ERROR)
    return(Gerror) ;
  surface_compiled = 0 ;
  inflated_surf_loaded = 1 ;
  return(NO_ERROR) ;
}
int
read_pial_vertex_coordinates(void)
{
  fprintf(stderr, "reading pial surface vertex locations...\n") ;
  /*  MRISsaveVertexPositions(mris, TMP_VERTICES) ;*/
  if (MRISreadVertexPositions(mris, "pial") != NO_ERROR)
    {
      fprintf(stderr, "could not read canonical surface from 'pial'\n") ;
      return(Gerror) ;
    }
  MRISsaveVertexPositions(mris, CANONICAL_VERTICES) ;
  /*  MRISrestoreVertexPositions(mris, TMP_VERTICES) ;*/
  pial_surf_loaded = 1 ;
  surface_compiled = 0 ;
  return(NO_ERROR) ;
}
void
show_surf(char *surf_name)
{
  if (!stricmp(surf_name, "white"))
    {
      if (!white_surf_loaded)
  read_white_vertex_coordinates() ;
      MRISrestoreVertexPositions(mris, ORIG_VERTICES) ;
    }
  else if (!stricmp(surf_name, "pial") || !stricmp(surf_name, "folded"))
    {
      if (!pial_surf_loaded)
  read_pial_vertex_coordinates() ;
      MRISrestoreVertexPositions(mris, CANONICAL_VERTICES) ;
    }
  else if (!stricmp(surf_name, "inflated"))
    {
      if (!inflated_surf_loaded)
  read_inflated_vertex_coordinates() ;
      MRISrestoreVertexPositions(mris, INFLATED_VERTICES) ;
    }
  else
    {
      fprintf(stderr, "unknown surface %s\n", surf_name) ;
      return ;
    }
  MRIScomputeNormals(mris) ;
  surface_compiled = 0 ; vertex_array_dirty = 1 ;
  redraw() ;
}  
int
read_orig_vertex_coordinates(char *fname)
{
  if (MRISreadOriginalProperties(mris, fname) == NO_ERROR)
    {
      origsurfloaded = TRUE ;
      MRISsaveVertexPositions(mris, TMP_VERTICES) ;
      MRISrestoreVertexPositions(mris, ORIGINAL_VERTICES) ;
      vset_save_surface_vertices( VSET_ORIGINAL );
      MRISrestoreVertexPositions(mris, TMP_VERTICES) ;
      enable_menu_set( MENUSET_VSET_ORIGINAL_LOADED, 1 );
    }
  else
    return(Gerror) ;
  return(NO_ERROR) ;
}
int
read_canon_vertex_coordinates(char *fname)
{
  MRISsaveVertexPositions(mris, TMP_VERTICES) ;
  if (MRISreadVertexPositions(mris, fname) == NO_ERROR)
    {
      MRISsaveVertexPositions(mris, CANONICAL_VERTICES) ;
      canonsurfloaded = TRUE ;
    }
  else
    canonsurffailed = TRUE ;
  
  MRISrestoreVertexPositions(mris, TMP_VERTICES) ;
  if (canonsurfloaded)
    return(NO_ERROR);
  else
    {
      canonsurffailed = TRUE ;
      return(Gerror) ;
    }
}

void
read_ellipsoid_vertex_coordinates(char *fname,float a,float b,float c)
{
#if 0
  int k,n,num,dummy;
  float x,y,z,ctrx,ctry,ctrz,phi,theta;
  int ix,iy,iz;
  int version;
  FILE *fp;
  int first;
  
  fp = fopen(fname,"r");
  if (fp==NULL) {printf("surfer: ### File %s not found\n",fname);PR return;}
  
  /* marty */
  fread3(&first,fp);
  if (first == QUAD_FILE_MAGIC_NUMBER) {
    version = -1;
    printf("surfer: new surface file format\n");
  }
  else {
    rewind(fp);
    version = 0;
    printf("surfer: old surface file format\n");
  }
  fread3(&mris->nvertices,fp);
  fread3(&mris->nfaces,fp);
  printf("surfer: read_ellipsoid_vertex_coordinates(%s,%f,%f,%f)\n",
         fname,a,b,c);
  printf("surfer: vertices=%d, faces=%d\n",mris->nvertices,mris->nfaces);
  ctrx = ctry = ctrz = 0;
  for (k=0;k<mris->nvertices;k++)
    {
      fread2(&ix,fp);
      fread2(&iy,fp);
      fread2(&iz,fp);
      x = ix/100.0;
      y = iy/100.0;
      z = iz/100.0;
      ctrx += x;
      ctry += y;
      ctrz += z;
    }
  ctrx /= mris->nvertices;
  ctry /= mris->nvertices;
  ctrz /= mris->nvertices;
  
  rewind(fp);
  fread3(&first,fp);
  if (first == QUAD_FILE_MAGIC_NUMBER) {
    version = -1;
  }
  else {
    rewind(fp);
    version = 0;
  }
  fread3(&mris->nvertices,fp);
  fread3(&mris->nfaces,fp);
  for (k=0;k<mris->nvertices;k++)
    {
      fread2(&ix,fp);
      fread2(&iy,fp);
      fread2(&iz,fp);
      x = ix/100.0-ctrx;
      y = iy/100.0-ctry;
      z = iz/100.0-ctrz;
      phi = asin((y/b)/sqrt(SQR(x/a)+SQR(y/b)+SQR(z/c)));
      theta = atan2(x/a,z/c);
      mris->vertices[k].val = phi*180/M_PI;
      mris->vertices[k].val2 = theta*180/M_PI;
      mris->vertices[k].coords[0] = phi*180/M_PI;
      mris->vertices[k].coords[1] = 2+theta*180/M_PI;
      mris->vertices[k].coords[2] = 0;
      if (version==0)
  {
    fread1(&num,fp);
    for (n=0;n<num;n++)
      fread3(&dummy,fp);
  }
    }
  fclose(fp);
  isocontourflag = TRUE;
  contour_spacing[0] = contour_spacing[1] = contour_spacing[2] = 10;
#endif
}

static float e0[3] = {0.08, -0.73, 0.67} ;
static float e1[3] = {0.57, 0.58, 0.57} ;
static float e2[3] = {-0.82, 0.34, 0.47} ;

void
find_orig_vertex_coordinates(int vindex)
{
  float x,y,z;
  char  fname[NAME_LENGTH];
  float x_tal, y_tal, z_tal ;
  FILE  *fp ;
  int   error = 0 ;
  
  if (vindex < 0 || vindex >= mris->nvertices)
    {
      fprintf(stderr, "no vertex selected.\n") ;
      return ;
    }
  
  x_tal = y_tal = z_tal = 0.0 ;
  
  if (origsurfloaded == FALSE)
    {
      printf("surfer: reading original coordinates from\n");
      printf("surfer:   %s\n",orfname);
    }
  /* read coordinates from .orig file and put them in the .tx fields */
  if (origsurfloaded == FALSE &&
      read_orig_vertex_coordinates(orfname) != NO_ERROR)
    {
      error = 1 ;
      printf("surfer: wrong number of vertices/faces in file %s\n",orfname);PR
                        printf("surfer: writing current coordinate (not orig) to file\n");PR
                                      x = mris->vertices[vindex].x ; 
      y = mris->vertices[vindex].y ; 
      z = mris->vertices[vindex].z ;
    }
  else  /* read file successfully */
    {
      x = mris->vertices[vindex].origx ; 
      y = mris->vertices[vindex].origy ; 
      z = mris->vertices[vindex].origz ;
    }
  if (transform_loaded) 
    LTAworldToWorld(lta, x, y, z, &x_tal, &y_tal, &z_tal) ;
  sprintf(fname,"%s/edit.dat",tfname);
  printf("writing coordinates to file %s\n", fname) ;
  fp = fopen(fname,"w");
  if (fp==NULL) 
    {
      printf("surfer: ### can't create file %s\n",fname);
      PR return;
    }
  printf("vertex %d coordinates:\n", vindex);
  if (!error)
    {
      if (transform_loaded) 
  printf("TALAIRACH (%2.1f %2.1f %2.1f)\n",x_tal,y_tal,z_tal);
      printf("ORIGINAL  (%2.1f %2.1f %2.1f)\n",x,y,z);
    }
  else
    printf("CURRENT   (%2.1f %2.1f %2.1f)\n",x,y,z);
  fprintf(fp,"%f %f %f\n",x,y,z);
  fprintf(fp,"%f %f %f\n",x_tal,y_tal,z_tal);
  fclose(fp);
  
  
  if (canonsurfloaded == FALSE)
    {
      sprintf(fname, "%s.sphere.reg", fpref) ;
      if (FileExists(fname))
  {
    printf("surfer: reading canonical coordinates from\n");
    printf("surfer:   %s\n",fname);
  }
    }
  
  if (canonsurfloaded == TRUE || (FileExists(fname) && 
          read_canon_vertex_coordinates(fname) == NO_ERROR))
    {
      float sx, sy, sz, r, d, phi, theta ;
      
      x = mris->vertices[vindex].cx ; 
      y = mris->vertices[vindex].cy ; 
      z = mris->vertices[vindex].cz ;
      sx = x*e0[0] + y*e0[1] + z*e0[2] ; 
      sy = x*e1[0] + y*e1[1] + z*e1[2] ; 
      sz = x*e2[0] + y*e2[1] + z*e2[2] ; 
      x = sx ; y = sy ; z = sz ;
      
      r = sqrt(x*x + y*y + z*z) ;
      d = r*r-z*z ; 
      if (d < 0.0) 
  d = 0.0 ;
      
      phi = atan2(sqrt(d), z) ;
      theta = atan2(y/r, x/r) ;
#if 0
#if 0
      if (theta < 0.0f)
  theta = 2 * M_PI + theta ;  /* make it 0 --> 2*PI */
#else
      if (theta > M_PI)
  theta -= 2 * M_PI ;  /* make it -PI --> PI */
#endif
#endif
      
      printf("SPHERICAL (%2.1f %2.1f %2.1f): (%2.1f, %2.1f)\n",sx,sy,sz,
       DEGREES(phi), DEGREES(theta));
    }
  PR
    }

void
select_talairach_point(int *vindex,float x_tal,float y_tal,float z_tal)
{
  float x,y,z;
  char fname[NAME_LENGTH];
  FILE *fp;
  
  if (!transform_loaded) 
    {
      printf("surfer: ### select_talairach_point failed: transform not "
       "loaded\n");
      PR return; 
    }
  
  LTAinverseWorldToWorld(lta, x_tal, y_tal, z_tal, &x, &y, &z) ;
  
  sprintf(fname,"%s/edit.dat",tfname);
  fp = fopen(fname,"w");
  if (fp==NULL) {printf("surfer: ### can't create file %s\n",fname);PR return;}
  fprintf(fp,"%f %f %f\n",x,y,z);
  fprintf(fp,"%f %f %f\n",x_tal,y_tal,z_tal);
  fclose(fp);
  
  select_orig_vertex_coordinates(vindex);
}

void
select_orig_vertex_coordinates(int *vindex)
{
  int   k;
  float d=0;
  float x,y,z,px,py,pz,mind;
  char  fname[NAME_LENGTH];
  FILE  *fp;
  
  sprintf(fname,"%s/edit.dat",tfname);
  fp = fopen(fname,"r");
  if (fp==NULL) {printf("surfer: ### File %s not found\n",fname);PR return;}
  fscanf(fp,"%f %f %f\n",&px,&py,&pz);
  fclose(fp);
  
  if (selection>=0) 
    draw_cursor(selection,FALSE);
  
  if (origsurfloaded == FALSE)
    {
      printf("surfer: reading coordinates from %s\n",orfname);
      read_orig_vertex_coordinates(orfname) ;
    }
  
  mind = 1e10;
  if (origsurfloaded == FALSE)
    {
      printf("surfer: ### wrong number of vertices/faces in file %s\n",fname);PR
                    printf("        using current position instead of orig...\n");PR
                                    for (k = 0 ; k < mris->nvertices ; k++)
                                      {
                                        x=mris->vertices[k].x; y = mris->vertices[k].y; z = mris->vertices[k].z;
                                        d = SQR(x-px)+SQR(y-py)+SQR(z-pz);
                                        if (d<mind) 
                                          {mind=d;*vindex=k;}
                                      }
    }
  else   /* find closest original vertex */
    {
      for (k=0;k<mris->nvertices;k++)
  {
    x = mris->vertices[k].origx ;
    y = mris->vertices[k].origy ;
    z = mris->vertices[k].origz ;
    d = SQR(x-px)+SQR(y-py)+SQR(z-pz);
    if (d<mind) 
      { mind=d;*vindex=k ; }
  }
    }
  printf("surfer: vertex %d: dist = %f\n",*vindex,sqrt(mind));PR;
  
  /* begin rkt */
  if (selection>=0)
    draw_cursor(selection,TRUE);
  if (k>=0)
    update_labels(LABELSET_CURSOR, k, d);
  /* end rkt */
  
  print_vertex_data(*vindex, stdout, sqrt(mind)) ;
}

/* print curv,icurv,icurvnei,=>cfact */
void
read_curvim_at_vertex(int vindex)
{
  VERTEX *v;
  int imnr,i,j,m,n;
  int delcurvdefined;
  float x,y,z,sx,sy,sz,sd;
  float dx,dy,dz;
  float xnei,ynei,znei;
  float curv,icurv,icurvnei,cfact;
  float icrange,crange;
  
  icurv = icurvnei = 0.0f ;
  if (!curvloaded)   { printf("surfer: ### curv not loaded!\n"); PR return; }
  if (!curvimloaded) { printf("surfer: ### curvim not loaded!\n"); PR return; }
  
  icrange = mris2->max_curv-mris2->min_curv;
  crange = mris->max_curv-mris->min_curv;
  
  v = &mris->vertices[vindex];
  x = v->x;
  y = v->y;
  z = v->z;
  sx=sy=sz=sd=0;
  n=0;
  delcurvdefined = TRUE;
  imnr = (int)((y-yy0)/st+0.5);
  i = (int)((zz1-z)/ps+0.5);
  j = (int)((xx1-x)/ps+0.5);
  imnr = (imnr<0)?0:(imnr>=numimg)?numimg-1:imnr;
  i = (i<0)?0:(i>=IMGSIZE)?IMGSIZE-1:i;
  j = (j<0)?0:(j>=IMGSIZE)?IMGSIZE-1:j;
  if (curvimflags[imnr][i][j] & CURVIM_DEFINED)
    icurv = bytetofloat(curvim[imnr][i][j],mris2->min_curv,mris2->max_curv);
  else
    delcurvdefined = FALSE;
  curv = v->curv;
  for (m=0;m<v->vnum;m++) {
    xnei = mris->vertices[v->v[m]].x;
    ynei = mris->vertices[v->v[m]].y;
    znei = mris->vertices[v->v[m]].z;
    imnr = (int)((ynei-yy0)/st+0.5);
    i = (int)((zz1-znei)/ps+0.5);
    j = (int)((xx1-xnei)/ps+0.5);
    imnr = (imnr<0)?0:(imnr>=numimg)?numimg-1:imnr;
    i = (i<0)?0:(i>=IMGSIZE)?IMGSIZE-1:i;
    j = (j<0)?0:(j>=IMGSIZE)?IMGSIZE-1:j;
    if (curvimflags[imnr][i][j] & CURVIM_DEFINED)
      icurvnei = bytetofloat(curvim[imnr][i][j],mris2->min_curv,mris2->max_curv);
    else
      delcurvdefined = FALSE;
    /*curvnei = mris->vertices[v->v[m]].curv;*/   /* two dels?? */
    /* del im w/only sign of im/loc diff */
    cfact = 1.0;
    if (delcurvdefined)
      cfact += (icurvnei-icurv)/icrange
  * copysign(icstrength,mris2->min_curv+curv*(icrange/crange) - icurv);
    sx += dx = (xnei - x)*cfact;
    sy += dy = (ynei - y)*cfact;
    sz += dz = (znei - z)*cfact;
    sd += sqrt(dx*dx+dy*dy+dz*dz);
    n++;
    if (delcurvdefined) {
      printf("surfer: ### nei vertex number: %d\n",n);PR
              printf("surfer: curv: %f\n",curv);PR
                          printf("surfer: icurv: %f\n",icurv);PR
                                  printf("surfer: icurvnei: %f\n",icurvnei);PR
                                                printf("surfer: cfact: %f\n",cfact);PR
                                                        }
  }
  if (!delcurvdefined) {
    printf("surfer: del curv not defined somewhere around this vertex\n");PR
                      }
}

#if 0
find_nearest_vertices(rad)   /* marty: get peaked distribution of nearest pts */
     float rad;
{
  FILE *fp;
  float xpt,ypt,zpt;
  float sqrad,sqdist;
  int k;
  
  fp=fopen(tfname,"r");
  if (fp==NULL) {printf("surfer: ### File %s not found\n",tfname);PR return;}
  fscanf(fp,"%f %f %f",&xpt,&ypt,&zpt);
  fclose(fp);
  sqrad = SQR(rad);
  
  /* read vertex coords from orig surface file */
  
  for (k=0;k<mris->nvertices;k++)
    {
      x = mris->vertices[k].x;
      y = mris->vertices[k].y;
      z = mris->vertices[k].z;
      sqdist = SQR(x-xpt) + SQR(y-ypt) + SQR(z-zpt);
      if (dist < sqrad) {
  mris->vertices[k].val = ;
  mris->vertices[k].val2 = ;
      }
    }
}
#endif

int
write_binary_surface(char *fname)
{
#if 1
  MRISwrite(mris, fname) ;
#else
  int k, type;
  float x,y,z;
  FILE *fp;
  
  type = mrisFileNameType(fname) ;
  if (type == MRIS_ASCII_QUADRANGLE_FILE)
    return(-1/*MRISwriteAscii(mris, fname)*/) ;
  else if (type == MRIS_GEO_TRIANGLE_FILE)
    return(-1/*MRISwriteGeo(mris, fname)*/) ;
  fp = fopen(fname,"w");
  if (fp==NULL) 
    {printf("surfer: ### can't create file %s\n",fname);PR return(-1);}
  else
    {
      return(MRISwriteTriangularSurface(fname)) ;
    }
  fwrite3(-1,fp);
  fwrite3(mris->nvertices,fp);
  fwrite3(mris->nfaces/2,fp);  /* # of quadrangles */
  for (k=0;k<mris->nvertices;k++)
    {
      x = mris->vertices[k].x;
      y = mris->vertices[k].y;
      z = mris->vertices[k].z;
      fwrite2((int)(x*100),fp);
      fwrite2((int)(y*100),fp);
      fwrite2((int)(z*100),fp);
    }
  for (k=0;k<mris->nfaces;k+=2)
    {
      fwrite3(mris->faces[k].v[0],fp);
      fwrite3(mris->faces[k].v[1],fp);
      fwrite3(mris->faces[k+1].v[0],fp);
      fwrite3(mris->faces[k].v[2],fp);
    }
  fclose(fp);
  printf("surfer: file %s written\n",fname);PR
#endif
                return(0) ;
}

void
write_binary_patch(char *fname)
{
#if 1
  MRISwritePatch(mris, fname) ;
#else
  int k,i,npts;
  float x,y,z;
  FILE *fp;
  
  npts = 0;
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag) npts++;
  printf("npts=%d\n",npts);
  fp = fopen(fname,"w");
  if (fp==NULL) {printf("surfer: ### can't create file %s\n",fname);PR return;}
  fwriteInt(npts,fp);
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag)
      {
  i = (mris->vertices[k].border)?-(k+1):k+1;
  fwriteInt(i,fp);
  x = mris->vertices[k].x;
  y = mris->vertices[k].y;
  z = mris->vertices[k].z;
  fwrite2((int)(x*100),fp);
  fwrite2((int)(y*100),fp);
  fwrite2((int)(z*100),fp);
  /*
    printf("k=%d, i=%d\n",k,i);
  */
      }
  fclose(fp);
#endif
}

void
read_binary_patch(char *fname)
{
  if (mris->patch)
    {
#if 1
      MRISunrip(mris) ;
#else
      char mris_fname[500] ;
      
      strcpy(mris_fname, mris->fname) ;
      MRISfree(&mris) ;
      mris = MRISread(mris_fname) ;
#endif
    }
  MRISreadPatchNoRemove(mris, fname) ;
  flag2d = TRUE;
  surface_compiled = 0 ;
  vertex_array_dirty = 1 ;
}

void
read_and_color_labeled_vertices(int r, int g, int b)
{
  meshr = r ; meshg = g ; meshb = b ;
  read_labeled_vertices(lfname) ;
  surface_compiled = 0 ;
}
/* begin rkt */

/* read_labeled_vertices now just calls labl_load to support multiple
   labels. */

#if 1 

void
read_labeled_vertices(char *fname)
{
  labl_load (fname);
}

#else

void
read_labeled_vertices(char *fname)
{
  if (area)
    LabelFree(&area) ;
  area = LabelRead(pname,fname) ;
  if (!area)
    return ;
  if (!origsurfloaded)
    read_orig_vertex_coordinates(orfname) ;
  LabelFillUnassignedVertices(mris, area) ;
  LabelMark(area, mris) ;
  surface_compiled = 0 ;
  redraw() ;
}
#endif

/* end rkt */

/* begin rkt */

/* replaced write_labeled_vertices here so that what used to be done
   in LabelFromMarkedSurfaces is now done here, so we can fill the
   area->lv[n].stat field with the current overlay value instead of
   v->stat. */
void
write_labeled_vertices(char *fname)
{
  int    vno, npoints, n ;
  VERTEX *v ;
  
  if (area)
    {
      if (origsurfloaded == FALSE)
  {
    fprintf(stderr, "reading original vertex locations...\n") ;
    MRISreadOriginalProperties(mris, NULL) ;
    origsurfloaded = TRUE ;
  }
      
      /* fill in the proper stat field here. */
      for (n = 0 ; n < area->n_points ; n++) 
  {
    sclv_get_value (&(mris->vertices[area->lv[n].vno]),
        sclv_current_field, &(area->lv[n].stat) );
  }
      
      fprintf(stderr, "writing %d labeled vertices to %s.\n",
        area ? area->n_points : -1, fname) ;
      LabelToOriginal(area, mris) ;
      LabelWrite(area, fname) ;
    }
  else
    {
      fprintf(stderr, "generating label from marked vertices...\n") ;
      
      /* count the number of marked vertices. */
      for (npoints = vno = 0 ; vno < mris->nvertices ; vno++)
  if (mris->vertices[vno].marked)
    npoints++ ;
      
      /* if we didn't get any, return. */
      if (!npoints) 
  {
    fprintf(stderr, "no marked vertices...\n") ;
    return;
  }
      
      /* allocate a label. */
      area = LabelAlloc(npoints, NULL, NULL) ;
      
      /* for every vertex, if it's marked, save its vertex coords,
   index, and fill the value of the current overlay. */
      for (n = vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (!v->marked)
      continue ;
    area->lv[n].x = v->x ;
    area->lv[n].y = v->y ;
    area->lv[n].z = v->z ;
    area->lv[n].vno = vno ;
    sclv_get_value (v, sclv_current_field, &(area->lv[n].stat) );
    n++ ;
  }
      area->n_points = npoints ;
      
      fprintf(stderr, "writing %d labeled vertices to %s.\n",
        area ? area->n_points : -1, fname) ;
      LabelToOriginal(area, mris) ;
      LabelWrite(area, fname) ;
      LabelFree(&area) ;
    }
}

#if 0
void
write_labeled_vertices(char *fname)
{
#if 1
  if (area)
    {
      if (origsurfloaded == FALSE)
  {
    fprintf(stderr, "reading original vertex locations...\n") ;
    MRISreadOriginalProperties(mris, NULL) ;
    origsurfloaded = TRUE ;
  }
      fprintf(stderr, "writing %d labeled vertices to %s.\n",
        area ? area->n_points : -1, fname) ;
      LabelToOriginal(area, mris) ;
      LabelWrite(area, fname) ;
    }
  else
    {
      fprintf(stderr, "generating label from marked vertices...\n") ;
      area = LabelFromMarkedSurface(mris) ;
      if (!area)
  {
    fprintf(stderr, "no marked vertices...\n") ;
    return ;
  }
      fprintf(stderr, "writing %d labeled vertices to %s.\n",
        area ? area->n_points : -1, fname) ;
      LabelToOriginal(area, mris) ;
      LabelWrite(area, fname) ;
      LabelFree(&area) ;
    }
#else
  int k,npts,asciiflag=TRUE;
  FILE *fp;
  
  read_orig_vertex_coordinates(orfname);
  npts = 0;
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag) npts++;
  printf("npts=%d\n",npts);
  fp = fopen(fname,"w");
  if (fp==NULL) {printf("surfer: ### can't create file %s\n",fname);PR return;}
  if (asciiflag)
    {
      fprintf(fp,"#!ascii\n");
      fprintf(fp,"%d\n",npts);
      for (k=0;k<mris->nvertices;k++)
  if (!mris->vertices[k].ripflag)
    fprintf(fp,"%d %f %f %f %f\n",
      k,mris->vertices[k].coords[0],mris->vertices[k].coords[1],
      mris->vertices[k].coords[2],mris->vertices[k].stat);
    } else
      {
  fputc('\0',fp);
  fwrite2(npts,fp);
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag)
      {
        fwriteInt(k,fp);
        fwriteFloat(mris->vertices[k].coords[0],fp);
        fwriteFloat(mris->vertices[k].coords[1],fp);
        fwriteFloat(mris->vertices[k].coords[2],fp);
        fwriteFloat(mris->vertices[k].stat,fp);
      }
      }
  fclose(fp);
  printf("surfer: file %s written\n",fname);PR
#endif
                }
#endif
/* end rkt */


void
write_binary_curvature(char *fname)
{
#if 1
  MRISwriteCurvature(mris, fname) ;
#else
  int k;
  FILE *fp;
  
  fp = fopen(fname,"w");
  if (fp==NULL) {printf("surfer: ### can't create file %s\n",fname);PR return;}
  fwrite3(mris->nvertices,fp);
  fwrite3(mris->nfaces,fp);
  for (k=0;k<mris->nvertices;k++)
    {
      fwrite2((int)(mris->vertices[k].curv*100),fp);
    }
  fclose(fp);
#endif
  printf("surfer: file %s written\n",fname);PR
                }

void
write_binary_areas(char *fname)
{
#if 1
  MRISwriteArea(mris, fname) ;
#else
  int k;
  FILE *fp;
  
  fp = fopen(fname,"w");
  if (fp==NULL) {printf("surfer: ### can't create file %s\n",fname);PR return;}
  fwrite3(mris->nvertices,fp);
  fwrite3(mris->nfaces,fp);
  for (k=0;k<mris->nvertices;k++)
    {
      fwriteFloat((mris->vertices[k].area),fp);
      mris->vertices[k].origarea = mris->vertices[k].area; 
    }
  fclose(fp);
#endif
  printf("surfer: file %s written\n",fname);PR
                }

/* 1/29/96: from paint.c */
void
write_binary_values(char *fname)
{
#if 1
  MRISwriteValues(mris, fname) ;
#else
  int k,num;
  float f;
  FILE *fp;
  double sum=0,sum2=0,max= -1000,min=1000;
  
  fp = fopen(fname,"wb");
  if (fp==NULL) {printf("surfer: ### can't create file %s\n",fname);PR return;}
  for (k=0,num=0;k<mris->nvertices;k++) if (mris->vertices[k].val!=0) num++;
  printf("num = %d\n",num);
  fwrite2(0,fp);
  fwrite3(num,fp);
  for (k=0;k<mris->nvertices;k++)
    {
      if (mris->vertices[k].val!=0)
  {
    fwrite3(k,fp);
    f = mris->vertices[k].val;
    fwriteFloat(f,fp);
    sum += f;
    sum2 += f*f;
    if (f>max) max=f;
    if (f<min) min=f;
  }
    }
  fclose(fp);
  sum /= num;
  sum2 = sqrt(sum2/num-sum*sum);
  printf("avg = %f, stdev = %f, min = %f, max = %f\n",sum,sum2,min,max);
#endif
  printf("surfer: file %s written\n",fname);PR
                
                }
void
read_stds(int cond_no)
{
  char  fname[STRLEN] ;
  int   dof, vno ;
  FILE  *fp  ;
  VERTEX *v ;
  double std ;
  
  sprintf(fname, "sigvar%d-%s.w", cond_no, stem) ;
  surface_compiled = 0 ;
  MRISreadValues(mris, fname) ;
  sprintf(fname, "sigavg%d.dof", cond_no) ;
  
  fp = fopen(fname, "r") ;
  if (!fp)
    {
      fprintf(stderr, "##tksurfer: could not open dof file %s\n", fname) ;
      return ;
    }
  fscanf(fp, "%d", &dof) ;
  fclose(fp) ;
  
  for (vno = 0 ; vno < mris->nvertices ; vno++)
    {
      v = &mris->vertices[vno] ;
      if (v->ripflag)
  continue ;
      std = sqrt(v->val * (float)dof) ;
      v->val = std ;
    }
}

#include "mriFunctionalDataAccess.h"
#include "mritransform.h"
#include "xVoxel.h"

/* begin rkt */
/* this is a dirty hack. i don't want to mess with read_binary_values too
 * too much. this version with the old signature read values into the v->val
 * field. it calls the new version passing SCLV_VAL. the new version is just
 * the old vesion with two differences. first, when reading a bfloat, it
 * switches on the field and sets the correct one. second, before calling
 * MRISreadValues, if the field is not SCLV_VAL, it saves all the v->vals,
 * and after, moves all the values in v->vals to the correct field, and
 * restores the values of v->val. something similar is done for 
 * read_binary_values_frame.
 */

void
read_binary_values(char *fname)
{
  sclv_read_binary_values (fname, SCLV_VAL);
}

int
sclv_read_binary_values(char *fname, int field)  /* marty: openclose */
{
  int                   vno ;
  int                   error_code;
  VERTEX                *v ;
  float*                saved_vals = NULL;
  char                  val_name[STRLEN];
  char                  cmd[STRLEN];
  float                 min, max;
  
#if 0
  char                  *dot, path[STRLEN], stem[STRLEN] ;
  Real                  x_ras, y_ras, z_ras ;
  mriFunctionalDataRef  statVolume ;
  xVoxel                voxel;
  FunD_tErr             errno ;
  float                 func_value;
  
  dot = strrchr(fname, '.') ;
  if (!dot)
    {
      char tmp[STRLEN] ;
      sprintf(tmp, "%s.w", fname) ;
      if (FileExists(tmp))
  dot = strrchr(tmp, '.') ;
    }
  if (!dot || !stricmp(dot+1, "float") || !stricmp(dot+1, "bfloat") || 
      !stricmp(dot+1, "bshort") || stricmp(dot+1, "w"))
    {
      error_code = read_orig_vertex_coordinates(paint_fname);
      if (NO_ERROR != error_code)
  return(error_code);
      
      trans_SetBounds ( xx0, xx1,yy0, yy1, zz0, zz1) ;
      trans_SetResolution(ps, ps, st) ;
      FileNamePath(fname, path) ;
      FileNameOnly(fname, stem) ;
      dot = strrchr(stem, '.') ;
      if (dot && (!stricmp(dot+1, "float") || !stricmp(dot+1, "bfloat") || 
      !stricmp(dot+1, "bshort")))
  {
    *dot = 0 ;
  }
      
      fprintf(stderr, "float volume detected - painting stats (%s, %s)...\n",
        path, stem) ;
      errno = FunD_New(&statVolume, path, stem, NULL, NULL) ;
      if (errno != FunD_tErr_NoError)
  {
    /*      MACRO_ReturnErrorOrCheckForQuestionable(errno,isQuestionable);*/
    ErrorReturn (ERROR_FUNC,
           (ERROR_FUNC,"error '%s' reading functional volume\n",
      FunD_GetErrorString(errno)));
  }
      
      for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (vno == Gdiag_no)
      DiagBreak() ;
    x_ras = v->origx ; y_ras = v->origy ; z_ras = v->origz ;
    xVoxl_SetFloat(&voxel, x_ras, y_ras, z_ras) ;
    if (FunD_GetDataAtRAS(statVolume, &voxel, 0, 0, &func_value) != 
        FunD_tErr_NoError)
      {
        sclv_set_value (v, field, func_value);
#if 0
        v->val = 0.0 ;
#endif
      } else {
        x_ras = v->x ;
      }
  }
    }
  else
    {
#endif
      
      /* unload this field if it already exists */
      sclv_unload_field (field);
      
      /* save all the v->val values */
      if (field != SCLV_VAL)
  {
    saved_vals = (float*) calloc (mris->nvertices, sizeof(float));
    if (saved_vals==NULL)
      ErrorReturn(ERROR_NOMEMORY,(ERROR_NOMEMORY,"sclv_read_binary_values: calloc with %d elmnts failed\n", mris->nvertices));
    
    for (vno = 0 ; vno < mris->nvertices ; vno++)
      {
        v = &mris->vertices[vno] ;
        saved_vals[vno] = v->val;
      }
  }
      
      /* save the directory for later */
      FileNamePath (fname, val_dir );
      
      /* read the file. if not found, bail. */
      error_code = MRISreadValues(mris, fname) ;
      if (error_code != NO_ERROR)
  {
    printf ("surfer: ### File %s could not be opened.\n", fname);
    return (error_code);
  }
      
      /* look for the min and max values. */
      min = 1000000;
      max = -1000000;
      for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    
    /* look for the min or max value */
    if (v->val < min)
      min = v->val;
    if (v->val > max)
      max = v->val;
  }
      
      /* move the v->vals into the proper field and restore the saved
   values. */
      if (field != SCLV_VAL && saved_vals != NULL)
  {
    for (vno = 0 ; vno < mris->nvertices ; vno++)
      {
        v = &mris->vertices[vno] ;
        
        /* look for the min or max value */
        if (v->val < min)
    min = v->val;
        if (v->val > max)
    max = v->val;
        
        /* set the field value from v->val */
        sclv_set_value (v, field, v->val);
        
        /* restore the value */
        v->val = saved_vals[vno];
      }
    free (saved_vals);
  }
      
#if 0
    }
#endif
  
  /* save the range */
  sclv_field_info[field].min_value = min;
  sclv_field_info[field].max_value = max;
  
  /* dummy info for time point and conditions, since .w files only have 
     one plane of info */
  sclv_field_info[field].cur_timepoint = 0;
  sclv_field_info[field].cur_condition = 0;
  sclv_field_info[field].num_timepoints = 1;
  sclv_field_info[field].num_conditions = 1;
  
  /* calc the frquencies */
  sclv_calc_frequencies (field);

  /* request a redraw. turn on the overlay flag and select this value set */
  vertex_array_dirty = 1 ;
  overlayflag = TRUE;
  sclv_set_current_field (field);
  
  /* set the field name to the name of the file loaded */
  if (NULL != g_interp)
    {
      FileNameOnly (fname, val_name);
      sprintf (cmd, "UpdateValueLabelName %d \"%s\"", field, val_name);
      Tcl_Eval (g_interp, cmd);
      sprintf (cmd, "ShowValueLabel %d 1", field);
      Tcl_Eval (g_interp, cmd);
      sprintf (cmd, "UpdateLinkedVarGroup view");
      Tcl_Eval (g_interp, cmd);
      sprintf (cmd, "UpdateLinkedVarGroup overlay");
      Tcl_Eval (g_interp, cmd);
    }
  
  /* enable the menu items */
  enable_menu_set (MENUSET_OVERLAY_LOADED, 1);
  
  return (ERROR_NONE);
}

int
sclv_read_bfile_values (char* dir, char* stem, char* registration, int field)
{
  FunD_tErr volume_error;
  mriFunctionalDataRef volume;
  char cmd[STRLEN];

  /* unload this field if it already exists */
  sclv_unload_field (field);
  
  /* create volume */
  volume_error = FunD_New (&volume, dir, stem, NULL, registration );
  if (volume_error!=FunD_tErr_NoError)
    {
      printf("surfer: couldn't load %s/%s",dir,stem);
      ErrorReturn(func_convert_error(volume_error),(func_convert_error(volume_error),"sclv_read_bfloat_values: error in FunD_New\n"));
    }
  
  /* save the volume and mark this field as binary */
  sclv_field_info[field].is_binary_volume = TRUE;
  sclv_field_info[field].volume = volume;
  
  /* get the range information */
  FunD_GetValueRange    (sclv_field_info[field].volume,
       &sclv_field_info[field].min_value,
       &sclv_field_info[field].max_value);
  FunD_GetNumConditions (sclv_field_info[field].volume, 
       &sclv_field_info[field].num_conditions);
  FunD_GetNumTimePoints (sclv_field_info[field].volume, 
       &sclv_field_info[field].num_timepoints);
  
  /* paint the first condition/timepoint in this field */
  sclv_set_timepoint_of_field (field, 0, 0);
  
  /* calc the frquencies */
  sclv_calc_frequencies (field);

  /* turn on the overlay flag and select this value set */
  vertex_array_dirty = 1 ;
  PR ;
  overlayflag = TRUE;
  sclv_set_current_field (field);
  
  /* set the field name to the name of the stem loaded */
  if (g_interp)
    {
      sprintf (cmd, "UpdateValueLabelName %d \"%s\"", field, stem);
      Tcl_Eval (g_interp, cmd);
      sprintf (cmd, "ShowValueLabel %d 1", field);
      Tcl_Eval (g_interp, cmd);
      sprintf (cmd, "UpdateLinkedVarGroup view");
      Tcl_Eval (g_interp, cmd);
      sprintf (cmd, "UpdateLinkedVarGroup overlay");
      Tcl_Eval (g_interp, cmd);
    }
  
  /* enable the menu items */
  enable_menu_set (MENUSET_OVERLAY_LOADED, 1);
  
  return (ERROR_NONE);
}

void
read_binary_values_frame(char *fname)
{
  sclv_read_binary_values_frame (fname, SCLV_VAL);
}

int
sclv_read_binary_values_frame(char *fname, 
            int field)  /* marty: open/leave open */
{
  int i,k,num;
  float f;
  float lat;
  VERTEX* v;
  
  surface_compiled = 0 ;
  
  if (!openvalfileflag) {
    fpvalfile = fopen(fname,"r");
    if (fpvalfile==NULL) 
      {
  printf("surfer: ### File %s not found\n",fname);
  PR 
    return(ERROR_NOFILE);
      }
    else
      openvalfileflag = TRUE;
  }
  
  fread2(&ilat,fpvalfile);
  lat = ilat/10.0;
  printf("surfer: latency = %6.1f\n",lat);
  
  for (k=0;k<mris->nvertices;k++)
    {
      v = &(mris->vertices[k]);
      sclv_set_value (v, field, 0);
#if 0    
      mris->vertices[k].val=0;
#endif
    }  
  
  fread3(&num,fpvalfile);
  printf("surfer: num=%d\n",num);
  for (i=0;i<num;i++)
    {
      fread3(&k,fpvalfile);
      f = freadFloat(fpvalfile);
      if (k>=mris->nvertices||k<0)
  printf("surfer: vertex index out of range: %d f=%f\n",k,f);
      else if (mris->vertices[k].d!=0)
  printf("surfer: subsample and data file mismatch\n");
      else
  {
    v = &(mris->vertices[k]);
    sclv_set_value (v, field, f);
#if 0    
    mris->vertices[k].val = f;
#endif
    mris->vertices[k].d=0;
  }
    }
  PR
    
    return (ERROR_NONE);
}

int
sclv_write_binary_values(char *fname, int field)
{
  float *saved_vals = NULL;
  VERTEX* v = NULL;
  int vno;
  
  /* first save all the current val values */
  if (field != SCLV_VAL)
    {
      saved_vals = (float*) calloc (mris->nvertices, sizeof(float));
      if (saved_vals==NULL)
  ErrorReturn(ERROR_NOMEMORY,(ERROR_NOMEMORY,"sclv_read_binary_values: calloc with %d elmnts failed\n", mris->nvertices));
      
      for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    
    /* save the value. */
    saved_vals[vno] = v->val;
    
    /* write the val from the requested field into val field */
    sclv_get_value (v, field, &(v->val));
  }
    }
  
  /* write the values */
  MRISwriteValues(mris, fname) ;
  
  /* restore the saved values */
  if (field != SCLV_VAL && saved_vals != NULL)
    {
      for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    v->val = saved_vals[vno];
  }
      free (saved_vals);
    }
  
  printf("surfer: file %s written\n",fname);PR
                
                return (ERROR_NONE);
}


void
swap_stat_val(void)
{
  sclv_swap_fields (SCLV_VAL, SCLV_VALSTAT);
  
#if 0  
  int k;
  float tmp;
  /* begin rkt */
  char cmd[STRLEN] ;
  /* end rkt */
  statflag = TRUE;
  for (k=0;k<mris->nvertices;k++)
    {
      tmp = mris->vertices[k].stat;
      mris->vertices[k].stat = mris->vertices[k].val;
      mris->vertices[k].val = tmp;
    }
  
  /* begin rkt */
  /* swap the names of the val and stat labels */
  sprintf (cmd, "SwapValueLabelNames %d %d", SCLV_VAL, SCLV_VALSTAT);
  Tcl_Eval (g_interp, cmd);
  /* end rkt */
#endif
}

void
swap_val_val2(void)
{
  sclv_swap_fields (SCLV_VAL, SCLV_VAL2);
  
#if 0
  int k;
  float tmp;
  
  for (k=0;k<mris->nvertices;k++)
    {
      tmp = mris->vertices[k].val2;
      mris->vertices[k].val2 = mris->vertices[k].val;
      mris->vertices[k].val = tmp ;
    }
  {
    char cmd[STRLEN] ;
    sprintf (cmd, "SwapValueLabelNames %d %d", SCLV_VAL, SCLV_VAL2);
    Tcl_Eval (g_interp, cmd);
  }
#endif
}

void
shift_values(void)   /* push one: val -> val2 */
{
  int k;
  
  for (k=0;k<mris->nvertices;k++)
    mris->vertices[k].val2 = mris->vertices[k].val;
  {
    char cmd[STRLEN] ;
    sprintf (cmd, "SwapValueLabelNames %d %d", SCLV_VAL, SCLV_VAL2);
    Tcl_Eval (g_interp, cmd);
  }
}

void
swap_values(void)   /* swap complex: val,valbak <-> val2,val2bak */
{
  sclv_swap_fields (SCLV_VAL, SCLV_VALBAK);
  sclv_swap_fields (SCLV_VAL2, SCLV_VAL2BAK);
  
#if 0
  int k;
  float h;
  
  for (k=0;k<mris->nvertices;k++)
    {
      h = mris->vertices[k].valbak;
      mris->vertices[k].valbak = mris->vertices[k].val;
      mris->vertices[k].val = h;
      h = mris->vertices[k].val2bak;
      mris->vertices[k].val2bak = mris->vertices[k].val2;
      mris->vertices[k].val2 = h;
    }
  {
    char cmd[STRLEN] ;
    sprintf (cmd, "SwapValueLabelNames %d %d", SCLV_VAL, SCLV_VALBAK);
    Tcl_Eval (g_interp, cmd);
    sprintf (cmd, "SwapValueLabelNames %d %d", SCLV_VAL2BAK, SCLV_VAL2);
    Tcl_Eval (g_interp, cmd);
  }
#endif
}

void
read_binary_decimation(char *fname)
{
  int vno ;
  surface_compiled = 0 ;
  sol_ndec = MRISreadDecimation(mris, fname) ;
  MRISclearMarks(mris) ;
  for (vno = 0 ; vno < mris->nvertices ; vno++)
    if (mris->vertices[vno].fixedval == TRUE)
      mris->vertices[vno].marked = 1 ;
  printf("surfer: decimation file %s read\n",fname);PR
                  }

void
write_binary_decimation(char *fname)
{
  MRISwriteDecimation(mris, fname) ;
  printf("surfer: decimation file %s written\n",fname);PR
}

void
write_decimation(char *fname)
{
  int k;
  FILE *fptr;
  
  fptr = fopen(fname,"w");
  if (fptr==NULL) {
    printf("surfer: ### can't create file %s\n",fname);PR return;}
  fprintf(fptr,"#!ascii\n");
  fprintf(fptr,"%d\n",mris->nvertices);
  for (k=0;k<mris->nvertices;k++)
    {
      if (mris->vertices[k].d==0)
  fprintf(fptr,"1\n");
      else
  fprintf(fptr,"0\n");
    }
  fclose(fptr);
  printf("surfer: decimation file %s written\n",fname);PR
               }

void
read_binary_dipoles(char *fname)
{
#if 0
  int k,d;
  char c;
  FILE *fptr;
  
  fptr = fopen(fname,"r");
  if (fptr==NULL) {
    printf("surfer: ### File %s not found\n",fname);PR return;}
  c = fgetc(fptr);
  if (c=='#')
    {
      fscanf(fptr,"%*s");
      fscanf(fptr,"%d",&d);
      if (d!=mris->nvertices) {
  printf("surfer: ### diople file mismatch %s\n",fname);PR return;}
      for (k=0;k<mris->nvertices;k++)
  {
    fscanf(fptr,"%f %f %f %f %f %f",
     &mris->vertices[k].dipx,&mris->vertices[k].dipy,&mris->vertices[k].dipz,
     &mris->vertices[k].dipnx,&mris->vertices[k].dipny,&mris->vertices[k].dipnz);
  }
    }
  else
    {
      d = freadInt(fptr);
      if (d!=mris->nvertices) {
  printf("surfer: ### dipole file mismatch %s\n",fname);PR return;}
      for (k=0;k<mris->nvertices;k++)
  {
    mris->vertices[k].dipx = freadFloat(fptr);
    mris->vertices[k].dipy = freadFloat(fptr);
    mris->vertices[k].dipz = freadFloat(fptr);
    mris->vertices[k].dipnx = freadFloat(fptr);
    mris->vertices[k].dipny = freadFloat(fptr);
    mris->vertices[k].dipnz = freadFloat(fptr);
  }
    }
  fclose(fptr);
  printf("surfer: dipole file %s read\n",fname);PR
#endif
              }

void
write_binary_dipoles(char *fname)
{
  int i,k;
  FILE *fptr;
  
  fptr = fopen(fname,"w");
  if (fptr==NULL) {
    printf("surfer: ### can't create file %s\n",fname);PR return;}
  fputc('\0',fptr);
  fwriteInt(mris->nvertices,fptr);
  for (k=0;k<mris->nvertices;k++)
    {
      fwriteFloat(mris->vertices[k].x,fptr);
      fwriteFloat(mris->vertices[k].y,fptr);
      fwriteFloat(mris->vertices[k].z,fptr);
      fwriteFloat(mris->vertices[k].nx,fptr);
      fwriteFloat(mris->vertices[k].ny,fptr);
      fwriteFloat(mris->vertices[k].nz,fptr);
    }
  for (k=0;k<mris->nvertices;k++)
    {
      fwriteInt(mris->vertices[k].vnum,fptr);
      for (i=0;i<mris->vertices[k].vnum;i++)
  fwriteInt(mris->vertices[k].v[i],fptr);
    }
  fclose(fptr);
  printf("surfer: dipole file %s written\n",fname);PR
                 }

void
write_dipoles(char *fname)
{
  int i,k;
  float x,y,z;
  FILE *fptr;
  
  fptr = fopen(fname,"w");
  if (fptr==NULL) {
    printf("surfer: ### can't create file %s\n",fname);PR return;}
  fprintf(fptr,"#!ascii\n");
  fprintf(fptr,"%d\n",mris->nvertices);
  for (k=0;k<mris->nvertices;k++)
    {
      x = mris->vertices[k].x;
      y = mris->vertices[k].y;
      z = mris->vertices[k].z;
      fprintf(fptr,"%6.2f %6.2f %6.2f %6.2f %6.2f %6.2f\n",
        x,y,z,
        mris->vertices[k].nx,mris->vertices[k].ny,mris->vertices[k].nz);
    }
  for (k=0;k<mris->nvertices;k++)
    {
      fprintf(fptr,"%d ",mris->vertices[k].vnum);
      for (i=0;i<mris->vertices[k].vnum;i++)
  fprintf(fptr,"%d ",mris->vertices[k].v[i]);
      fprintf(fptr,"\n");
    }
  fclose(fptr);
  printf("surfer: dipole file %s written\n",fname);PR
                 }

void
write_subsample(char *fname)
{
  int k;
  float x,y,z;
  FILE *fptr;
  
  fptr = fopen(fname,"w");
  if (fptr==NULL) {
    printf("surfer: ### can't create file %s\n",fname);PR return;}
  fprintf(fptr,"%d\n",sub_num);
  for (k=0;k<mris->nvertices;k++)
    {
      if (mris->vertices[k].d==0)
  {
    x = mris->vertices[k].x;
    y = mris->vertices[k].y;
    z = mris->vertices[k].z;
    fprintf(fptr,"%d %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f\n",
      k,
      x,y,z,
      mris->vertices[k].nx,mris->vertices[k].ny,mris->vertices[k].nz);
  }
    }
  fclose(fptr);
}

void
write_binary_subsample(char *fname)
{
  int k;
  float x,y,z,nx,ny,nz;
  FILE *fp;
  
  fp = fopen(fname,"w");
  if (fp==NULL) {printf("surfer: ### can't create file %s\n",fname);PR return;}
  fwrite3(sub_num,fp);
  for (k=0;k<mris->nvertices;k++)
    {
      if (mris->vertices[k].d==0)
  {
    x = mris->vertices[k].x;
    y = mris->vertices[k].y;
    z = mris->vertices[k].z;
    nx = mris->vertices[k].nx;
    ny = mris->vertices[k].ny;
    nz = mris->vertices[k].nz;
    fwrite3(k,fp);
    fwrite2((int)(x*100),fp);
    fwrite2((int)(y*100),fp);
    fwrite2((int)(z*100),fp);
    fwrite2((int)(nx*10000),fp);
    fwrite2((int)(ny*10000),fp);
    fwrite2((int)(nz*10000),fp);
  }
    }
  fclose(fp);
  printf("surfer: file %s written\n",fname);PR
                }

void
read_binary_subsample(char *fname)
{
  int i,k;
  int ix,iy,iz,inx,iny,inz;
  FILE *fp;
  
  fp = fopen(fname,"r");
  if (fp==NULL) {printf("surfer: ### File %s not found\n",fname);PR return;}
  fread3(&sub_num,fp);
  for (k=0;k<mris->nvertices;k++)
    mris->vertices[k].d = 1;
  for (i=0;i<sub_num;i++)
    {
      fread3(&k,fp);
      fread2(&ix,fp);
      fread2(&iy,fp);
      fread2(&iz,fp);
      fread2(&inx,fp);
      fread2(&iny,fp);
      fread2(&inz,fp);
      mris->vertices[k].d = 0;
    }
  fclose(fp);
}

void
read_binary_curvature(char *fname)
{
#if 1
  int    vno, n ;
  VERTEX *v ;
  
  surface_compiled = 0 ;
  MRISreadCurvatureFile(mris, fname) ;
  for (dipavg = 0.0, n = vno = 0 ; vno < mris->nvertices ; vno++)
    {
      v = &mris->vertices[vno] ;
      if (v->ripflag)
  continue ;
      dipavg += v->curv ; n++ ;
    }
  dipavg /= (float)n ;
#else
  int k,i,vnum,fnum;
  float curv;
  FILE *fp;
  
  fp = fopen(fname,"r");
  if (fp==NULL) {printf("surfer: ### File %s not found\n",fname);PR return;}
  fread3(&vnum,fp);
  fread3(&fnum,fp);
  if (vnum!=mris->nvertices)
    {
      printf("surfer: ### incompatible vertex number in file %s\n",fname);PR
                      return;
    }
  for (k=0;k<vnum;k++)
    {
      fread2(&i,fp);
      curv = i/100.0;
      if (k==0) mris->min_curv=mris->max_curv=curv;
      if (curv>mris->max_curv) mris->max_curv=curv;
      if (curv<mris->min_curv) mris->min_curv=curv;
      mris->vertices[k].curv = curv;
    }
  fclose(fp);
#endif
  printf("surfer: curvature read: min=%f max=%f\n",mris->min_curv,mris->max_curv);PR
                        curvloaded = TRUE;
  /* begin rkt */
  /* save the curv min and max. */
  cmin = mris->min_curv;
  cmax = mris->max_curv;

  /* enable our menu options. */
  enable_menu_set (MENUSET_CURVATURE_LOADED, curvloaded);
  if (g_interp)
    Tcl_Eval (g_interp, "ShowLabel kLabel_Curvature 1");

  /* turn on the curvflag */
  curvflag = 1;

  /* end rkt */
} 

void
normalize_binary_curvature(void)
{
  int k;
  float curv,min,max;
  float sum,avg,sum_sq,sd,n;
  
  min = max = 0.0f ;
  if (!curvloaded)   { printf("surfer: ### curv not loaded!\n");PR return; }
  
  sum = 0;
  for (k=0;k<mris->nvertices;k++)
    sum += mris->vertices[k].curv;
  avg = sum/mris->nvertices;
  
  n = (float)mris->nvertices;
  sum = sum_sq = 0.0;
  for (k=0;k<mris->nvertices;k++) {
    mris->vertices[k].curv -= avg;
    curv = mris->vertices[k].curv;
    sum += curv;
    sum_sq += curv*curv;
  }
  sd = sqrt((n*sum_sq - sum*sum)/(n*(n-1.0)));
  
  for (k=0;k<mris->nvertices;k++) {
    curv = (mris->vertices[k].curv)/sd;
    if (k==0) min=max=curv;
    if (curv<min) min=curv;
    if (curv>max) max=curv;
    if (curv<CURVIM_NORM_MIN) curv = CURVIM_NORM_MIN;
    if (curv>CURVIM_NORM_MAX) curv = CURVIM_NORM_MAX;
    mris->vertices[k].curv = curv;
  }
  mris->min_curv = CURVIM_NORM_MIN;
  mris->max_curv = CURVIM_NORM_MAX;
  printf("surfer: curvature normalized: avg=%f sd=%f\n",avg,sd);
  printf("surfer: min=%f max=%f trunc to (%f,%f)\n",min,max,mris->min_curv,mris->max_curv);PR
                           }

void
read_binary_areas(char *fname)
{
#if 1
  surface_compiled = 0 ;
  MRISreadBinaryAreas(mris, fname) ;
#else
  int k,vnum,fnum;
  float f;
  FILE *fp;
  
  mris->total_area = 0;
  fp = fopen(fname,"r");
  if (fp==NULL) { printf("surfer: no area file %s\n",fname);PR return; }
  fread3(&vnum,fp);
  fread3(&fnum,fp);
  if (vnum!=mris->nvertices)
    {
      printf("surfer: ### incompatible vertex number in file %s\n",fname);PR
                      printf("   ...file ignored\n");PR
                               return;
    }
  for (k=0;k<vnum;k++)
    {
      f = freadFloat(fp);
      mris->vertices[k].origarea = f;
      mris->total_area += f;
    }
  fclose(fp);
  mris->total_area /= 2; /* hack to correct overest. of area in compute_normals */
#endif
}

void
read_fieldsign(char *fname)
{
  int k,vnum;
  float f;
  FILE *fp;
  
  printf("surfer: read_fieldsign(%s)\n",fname);
  fp = fopen(fname,"r");
  if (fp==NULL) {printf("surfer: ### File %s not found\n",fname);PR return;}
  vnum = freadInt(fp);
  printf("surfer: mris->nvertices = %d, vnum = %d\n",mris->nvertices,vnum);
  if (vnum!=mris->nvertices) {
    printf("surfer: ### incompatible vertex number in file %s\n",fname);
    printf("   ...file read anyway\n");PR
           }
  for (k=0;k<vnum;k++)
    {
      f = freadFloat(fp);
      mris->vertices[k].fieldsign = f;
    }
  fclose(fp);
  fieldsignflag = TRUE;
  PR
    }

void
write_fieldsign(char *fname)
{
  int k,vnum;
  float f;
  FILE *fp;
  
  vnum = mris->nvertices;
  fp = fopen(fname,"w");
  if (fp==NULL) 
    {printf("surfer: ### can't create file %s\n",fname);PR return;}
  fwriteInt(vnum,fp);
  printf("surfer: mris->nvertices = %d, vnum = %d\n",mris->nvertices,vnum);
  for (k=0;k<vnum;k++)
    {
      f = mris->vertices[k].fieldsign;
      fwriteFloat(f,fp);
    }
  fclose(fp);
  printf("surfer: file %s written\n",fname);PR
                }

void
read_fsmask(char *fname)
{
  int k,vnum;
  float f;
  FILE *fp;
  
  printf("surfer: read_fsmask(%s)\n",fname);
  fp = fopen(fname,"r");
  if (fp==NULL) {printf("surfer: ### File %s not found\n",fname);PR return;}
  vnum = freadInt(fp);
  if (vnum!=mris->nvertices)
    printf("surfer: warning: incompatible vertex number in file %s\n",fname);
  for (k=0;k<vnum;k++)
    {
      f = freadFloat(fp);
      mris->vertices[k].fsmask = f;
    }
  fclose(fp);
  PR
    }

void
write_fsmask(char *fname)
{
  int k,vnum;
  float f;
  FILE *fp;
  
  vnum = mris->nvertices;
  fp = fopen(fname,"w");
  if (fp==NULL) {printf("surfer: ### can't create file %s\n",fname);PR return;}
  fwriteInt(vnum,fp);
  for (k=0;k<vnum;k++)
    {
      f = mris->vertices[k].fsmask;
      fwriteFloat(f,fp);
    }
  fclose(fp);
  printf("surfer: file %s written\n",fname);PR
                }

void
write_vrml(char *fname,int mode)
{
  FILE *fp;
  int k,n;
  
  fp = fopen(fname,"w");
  if (fp==NULL) {printf("surfer: ### can't create file %s\n",fname);PR return;}
  
  if (mode==VRML_POINTS) {
    /* color here */
    if (vrml2flag) {
      fprintf(fp,"#VRML V2.0 utf8\n");
      fprintf(fp,"Collision { collide FALSE\n");
      fprintf(fp,"children [ Group { children [ Shape {\n");
      fprintf(fp,"appearance Appearance { material DEF _DefMat Material { } }\n");
      fprintf(fp,"geometry PointSet { coord Coordinate {\n");
    }
    else {
      fprintf(fp,"#VRML V1.0 ascii\n");
      fprintf(fp,"Coordinate3 {\n");
    }
    
    fprintf(fp,"\tpoint [\n");
    for (k=0;k<mris->nvertices;k++) {
      fprintf(fp,"%3.2f %3.2f %3.2f",mris->vertices[k].x,mris->vertices[k].y,mris->vertices[k].z);
      if (k!=mris->nvertices-1) fprintf(fp,",\n");
    }
    fprintf(fp,"\n\t]\n");
    
    if (vrml2flag) {
      fprintf(fp,"} } } ] } ] }\n");
    }
    else {
      fprintf(fp,"}\n");
      fprintf(fp,"PointSet {\n");
      fprintf(fp,"\tstartIndex 0\n");
      fprintf(fp,"\tnumPoints -1\n");
      fprintf(fp,"}\n");
    }
  }
  if (mode==VRML_QUADS_GRAY || mode==VRML_QUADS_GRAY_SMOOTH) {
    fprintf(fp,"#VRML V1.0 ascii\n");
    fprintf(fp,"Coordinate3 {\n");
    fprintf(fp,"\tpoint [\n");
    for (k=0;k<mris->nvertices;k++) {
      fprintf(fp,"%3.2f %3.2f %3.2f",mris->vertices[k].x,mris->vertices[k].y,mris->vertices[k].z);
      if (k!=mris->nvertices-1) fprintf(fp,",\n");
    }
    fprintf(fp,"\n\t]\n");
    fprintf(fp,"}\n");
    
    if (mode==VRML_QUADS_GRAY_SMOOTH) {
      fprintf(fp,"ShapeHints {\n");
      /*fprintf(fp,"\tvertexOrdering COUNTERCLOCKWISE\n");*/
      /*fprintf(fp,"\tshapeType SOLID\n");*/  /* no backface speedup */
      fprintf(fp,"\tcreaseAngle 0.8\n");
      fprintf(fp,"}\n");
    }
    
    fprintf(fp,"IndexedFaceSet {\n");
    fprintf(fp,"\tcoordIndex [\n");
    for (k=0;k<mris->nfaces;k++) {
      for (n=0;n<VERTICES_PER_FACE;n++) {
        fprintf(fp,"%d",mris->faces[k].v[n]);
        if (n<3) { fprintf(fp,", "); }
        else     { if (k!=mris->nfaces-1) fprintf(fp,", -1,\n"); }
      }
    }
    fprintf(fp,"\n\t]\n");
    fprintf(fp,"}\n");
  }
  if (mode==VRML_QUADS_CURV) {
    if (!curvloaded) read_binary_curvature(cfname);
#if 0
    /*** TODO ***/
    fprintf(fp,"#VRML V1.0 ascii\n");
    fprintf(fp,"Coordinate3 {\n");
    fprintf(fp," point [\n");
    for (k=0;k<mris->nvertices;k++) {
      fprintf(fp,"%3.2f %3.2f %3.2f",mris->vertices[k].x,mris->vertices[k].y,mris->vertices[k].z);
      if (k!=mris->nvertices-1) fprintf(fp,",\n");
    }
    fprintf(fp," ]\n");
    fprintf(fp,"}\n");
    
    fprintf(fp,"Material {\n");
    fprintf(fp," diffuseColor [\n");
    fprintf(fp," ]\n");
    fprintf(fp,"}\n");
    
    fprintf(fp,"MaterialBinding {\n");
    fprintf(fp," value PER_VERTEX\n"); /* kill overlap: PER_MRIS->NVERTICESED */
    fprintf(fp,"}\n");
    
    fprintf(fp,"IndexedFaceSet {\n");
    fprintf(fp," coordIndex [\n");
    for (k=0;k<mris->nfaces;k++) {
      for (n=0;n<VERTICES_PER_FACE;n++) {
        fprintf(fp,"%d",mris->faces[k].v[n]);
        if (n<3) { fprintf(fp,", "); }
        else     { if (k!=mris->nfaces-1) fprintf(fp,", -1,\n"); }
      }
    }
    fprintf(fp," ]\n");
    fprintf(fp,"}\n");
    fprintf(fp," materialIndex [\n");
    /* ... */
    fprintf(fp," ]\n");
    fprintf(fp,"}\n");
#endif
  }
  fclose(fp);
  if (vrml2flag) printf("surfer: vrml 2.0 file %s written",fname);
  else           printf("surfer: vrml 1.0 file %s written",fname);
  PR
    }

void
rip_faces(void)
{
  int n,k;
  FACE *f;
  
  for (k=0;k<mris->nfaces;k++)
    mris->faces[k].ripflag = FALSE;
  for (k=0;k<mris->nfaces;k++)
    {
      f = &mris->faces[k];
      for (n=0;n<VERTICES_PER_FACE;n++)
  if (mris->vertices[f->v[n]].ripflag)
    f->ripflag = TRUE;
    }
  for (k=0;k<mris->nvertices;k++)
    mris->vertices[k].border = FALSE;
  for (k=0;k<mris->nfaces;k++)
    if (mris->faces[k].ripflag)
      {
  f = &mris->faces[k];
  for (n=0;n<VERTICES_PER_FACE;n++)
    mris->vertices[f->v[n]].border = TRUE;
      }
}

void
normalize(float v[3])
{
  float d;
  
  d = sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
  if (d>0)
    {
      v[0] /= d;
      v[1] /= d;
      v[2] /= d;
    }
}

void
normal_face(int fac,int n,float *norm)
{
  int n0,n1;
  FACE *f;
  float v0[3],v1[3];
  
  n0 = (n==0)                   ? VERTICES_PER_FACE-1 : n-1;
  n1 = (n==VERTICES_PER_FACE-1) ? 0                   : n+1;
  f = &mris->faces[fac];
  v0[0] = mris->vertices[f->v[n]].x - mris->vertices[f->v[n0]].x;
  v0[1] = mris->vertices[f->v[n]].y - mris->vertices[f->v[n0]].y;
  v0[2] = mris->vertices[f->v[n]].z - mris->vertices[f->v[n0]].z;
  v1[0] = mris->vertices[f->v[n1]].x - mris->vertices[f->v[n]].x;
  v1[1] = mris->vertices[f->v[n1]].y - mris->vertices[f->v[n]].y;
  v1[2] = mris->vertices[f->v[n1]].z - mris->vertices[f->v[n]].z;
  normalize(v0);
  normalize(v1);
  norm[0] = -v1[1]*v0[2] + v0[1]*v1[2];
  norm[1] = v1[0]*v0[2] - v0[0]*v1[2];
  norm[2] = -v1[0]*v0[1] + v0[0]*v1[1];
  /*
    printf("[%5.2f,%5.2f,%5.2f] x [%5.2f,%5.2f,%5.2f] = [%5.2f,%5.2f,%5.2f]\n",
    v0[0],v0[1],v0[2],v1[0],v1[1],v1[2],norm[0],norm[1],norm[2]);
  */
}

float triangle_area(int fac,int n)
{
  int n0,n1;
  FACE *f;
  float v0[3],v1[3],d1,d2,d3;
  
  n0 = (n==0)                   ? VERTICES_PER_FACE-1 : n-1;
  n1 = (n==VERTICES_PER_FACE-1) ? 0                   : n+1;
  f = &mris->faces[fac];
  v0[0] = mris->vertices[f->v[n]].x - mris->vertices[f->v[n0]].x;
  v0[1] = mris->vertices[f->v[n]].y - mris->vertices[f->v[n0]].y;
  v0[2] = mris->vertices[f->v[n]].z - mris->vertices[f->v[n0]].z;
  v1[0] = mris->vertices[f->v[n1]].x - mris->vertices[f->v[n]].x;
  v1[1] = mris->vertices[f->v[n1]].y - mris->vertices[f->v[n]].y;
  v1[2] = mris->vertices[f->v[n1]].z - mris->vertices[f->v[n]].z;
  d1 = -v1[1]*v0[2] + v0[1]*v1[2];
  d2 = v1[0]*v0[2] - v0[0]*v1[2];
  d3 = -v1[0]*v0[1] + v0[0]*v1[1];
  return sqrt(d1*d1+d2*d2+d3*d3)/2;
}

#define MAX_NEIGHBORS 300  /* ridiculously large */

void
find_neighbors(void)
{
  int n0,n1,i,k,m,n;
  FACE *f;
  VERTEX *v;
  int vtmp[MAX_NEIGHBORS];
  
  for (k=0;k<mris->nvertices;k++)
    {
      v = &mris->vertices[k];
      v->vnum = 0;
      for (m=0;m<v->num;m++)
  {
    n = v->n[m];
    f = &mris->faces[v->f[m]];
    n0 = (n==0)                   ? VERTICES_PER_FACE-1 : n-1;
    n1 = (n==VERTICES_PER_FACE-1) ? 0                   : n+1;
    for (i=0;i<v->vnum && vtmp[i]!=f->v[n0];i++);
    if (i==v->vnum)
      vtmp[(int)v->vnum++] = f->v[n0];
    for (i=0;i<v->vnum && vtmp[i]!=f->v[n1];i++);
    if (i==v->vnum)
      vtmp[(int)v->vnum++] = f->v[n1];
  }
      mris->vertices[k].v = (int *)lcalloc(mris->vertices[k].vnum,sizeof(int));
      for (i=0;i<v->vnum;i++)
  {
    v->v[i] = vtmp[i];
  }
      /*
  if (v->num != v->vnum)
  printf("%d: num=%d vnum=%d\n",k,v->num,v->vnum);
      */
    }
  for (k=0;k<mris->nfaces;k++)
    {
      f = &mris->faces[k];
      for (m=0;m<VERTICES_PER_FACE;m++)
  {
    v = &mris->vertices[f->v[m]];
    for (i=0;i<v->num && k!=v->f[i];i++);
    if (i==v->num)
      printf("mris->faces[%d].v[%d] = %d\n",k,m,f->v[m]);
  }
    }
}

void
compute_normals(void)  /* no triangle area in msurfer, no explodeflag here */
{
  int k,n;
  VERTEX *v;
  FACE *f;
  float norm[3],snorm[3];
  
  for (k=0;k<mris->nfaces;k++)
    if (mris->faces[k].ripflag)
      {
  f = &mris->faces[k];
  for (n=0;n<VERTICES_PER_FACE;n++)
    mris->vertices[f->v[n]].border = TRUE;
      }
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag)
      {
  v = &mris->vertices[k];
  snorm[0]=snorm[1]=snorm[2]=0;
  v->area = 0;
  for (n=0;n<v->num;n++)
    if (!mris->faces[v->f[n]].ripflag)
      {
        normal_face(v->f[n],v->n[n],norm);
        snorm[0] += norm[0];
        snorm[1] += norm[1];
        snorm[2] += norm[2];
        v->area += triangle_area(v->f[n],v->n[n]); /* Note: overest. area by 2! */
      }
  normalize(snorm);
  
  if (v->origarea<0)
    v->origarea = v->area;
  
  v->nx = snorm[0];
  v->ny = snorm[1];
  v->nz = snorm[2];
      }
}

void
compute_shear(void)
{
#if 0
  int k,n,sumn;
  float x0,y0,x1,y1,x2,y2,x3,y3;
  float x01,y01,x12,y12,x23,y23,x30,y30,x02,y02,x13,y13;
  float d01,d12,d23,d30,d02,d13,d,r,r1,r2,shearx,sheary;
  float sx,sy,dsx,dsy,dshearx,dsheary;
  FACE *f;
  VERTEX *v;
  
  for (k=0;k<mris->nfaces;k++)
    if (!mris->faces[k].ripflag)
      {
  f = &mris->faces[k];
  x0 = mris->vertices[f->v[0]].x; 
  y0 = mris->vertices[f->v[0]].y; 
  x1 = mris->vertices[f->v[1]].x; 
  y1 = mris->vertices[f->v[1]].y; 
  x2 = mris->vertices[f->v[2]].x; 
  y2 = mris->vertices[f->v[2]].y; 
#if VERTICES_PER_FACE == 4
  x3 = mris->vertices[f->v[3]].x; 
  y3 = mris->vertices[f->v[3]].y; 
#else
  /* don't know what to do here!! */
  x3 = mris->vertices[f->v[0]].x; 
  y3 = mris->vertices[f->v[0]].y; 
#endif
  x01 = x1-x0; 
  y01 = y1-y0; 
  x12 = x2-x1; 
  y12 = y2-y1; 
  x23 = x3-x2; 
  y23 = y3-y2; 
  x30 = x0-x3; 
  y30 = y0-y3; 
  x02 = x2-x0; 
  y02 = y2-y0; 
  x13 = x3-x1; 
  y13 = y3-y1; 
  d01 = sqrt(x01*x01+y01*y01);
  d12 = sqrt(x12*x12+y12*y12);
  d23 = sqrt(x23*x23+y23*y23);
  d30 = sqrt(x30*x30+y30*y30);
  d02 = sqrt(x02*x02+y02*y02);
  d13 = sqrt(x13*x13+y13*y13);
  r1 = (((d01+d23)*(d12+d30))>0)?log((d01+d23)/(d12+d30)):0;
  r2 = ((d02*d13)>0)?log(d02/d13):0;
  if (fabs(r1)>fabs(r2))
    {
      r = fabs(r1);
      if (r1>0)
        {
    shearx = (x01-x23-y12+y30);
    sheary = (y01-y23+x12-x30);
        }
      else
        {
    shearx = (x12-x30+y01-y23);
    sheary = (y12-y30-x01+x23);
        }
    }
  else
    {
      r = fabs(r2);
      if (r2>0)
        {
    shearx = (x02-y13);
    sheary = (y02+x13);
        }
      else
        {
    shearx = (x13+y02);
    sheary = (y13-x02);
        }
    }
  d = sqrt(shearx*shearx+sheary*sheary);
  /*
    printf("k=%d: r1=%f, r2=%f, r=%f, shear=(%f,%f)\n",k,r1,r2,r,shearx,sheary);
  */
  shearx = (d>0)?r*shearx/d:0;
  sheary = (d>0)?r*sheary/d:0;
  f->logshear = r; 
  f->shearx = shearx; 
  f->sheary = sheary; 
      }
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag)
      {
  v = &mris->vertices[k];
  dshearx = dsheary = sumn = 0;
  for (n=0;n<v->num;n++)
    if (!mris->faces[v->f[n]].ripflag)
      {
        sx = mris->faces[v->f[n]].shearx;
        sy = mris->faces[v->f[n]].sheary;
        double_angle(sx,sy,&dsx,&dsy);
        dshearx += dsx;
        dsheary += dsy;
        sumn++;
      }
  dshearx = (sumn>0)?dshearx/sumn:0;
  dsheary = (sumn>0)?dsheary/sumn:0;
  halve_angle(dshearx,dsheary,&shearx,&sheary);
  mris->vertices[k].shearx = shearx;
  mris->vertices[k].sheary = sheary;
  r = sqrt(shearx*shearx+sheary*sheary);
  mris->vertices[k].logshear = r;
  /*
    printf("k=%d: logshear=%f shear=(%f,%f)\n",k,r,shearx,sheary);
  */
      }
#endif
}
void
double_angle(float x,float y,float *dx,float *dy)
{
  float a,d;
  
  d = sqrt(x*x+y*y);
  a = atan2(y,x);
  *dx = d*cos(2*a); 
  *dy = d*sin(2*a); 
  /*
    printf("double_angle(%5.2f,%5.2f,%5.2f,%5.2f):d=%5.2f,a=%6.1f,2*a=%6.1f\n",
    x,y,*dx,*dy,d,a*180/M_PI,2*a*180/M_PI);
  */
}

void
halve_angle(float x,float y,float *dx,float *dy)
{
  float a,d;
  
  d = sqrt(x*x+y*y);
  a = atan2(y,x);
  *dx = d*cos(a/2); 
  *dy = d*sin(a/2); 
  /*
    printf("halve_angle(%5.2f,%5.2f,%5.2f,%5.2f):d=%5.2f,a=%6.1f,a/2=%6.1f\n",
    x,y,*dx,*dy,d,a*180/M_PI,a/2*180/M_PI);
  */
}

void
compute_boundary_normals(void)
{
#if 0
  int k,m,n;
  VERTEX *v;
  float sumx,sumy,r,nx,ny,f;
  
  for (k=0;k<mris->nvertices;k++)
    if ((!mris->vertices[k].ripflag)&&mris->vertices[k].border)
      {
  v = &mris->vertices[k];
  n = 0;
  sumx = 0;
  sumy = 0;
  for (m=0;m<v->vnum;m++)
    if (!mris->vertices[v->v[m]].ripflag)
      {
        sumx += v->x-mris->vertices[v->v[m]].x;
        sumy += v->y-mris->vertices[v->v[m]].y;
        n++;
      }
  v->bnx = (n>0)?sumx/n:0;
  v->bny = (n>0)?sumy/n:0;
      }
  for (k=0;k<mris->nvertices;k++)
    if ((!mris->vertices[k].ripflag)&&mris->vertices[k].border)
      {
  v = &mris->vertices[k];
  n = 0;
  sumx = 0;
  sumy = 0;
  for (m=0;m<v->vnum;m++)
    if ((!mris->vertices[v->v[m]].ripflag)&&mris->vertices[v->v[m]].border)
      {
        nx = -(v->y-mris->vertices[v->v[m]].y); 
        ny = v->x-mris->vertices[v->v[m]].x; 
        f = nx*v->bnx+ny*v->bny;
        /*
    f = (f<0)?-1.0:(f>0)?1.0:0.0;
        */
        sumx += f*nx;
        sumy += f*ny;
        n++;
      }
  v->bnx = (n>0)?sumx/n:0;
  v->bny = (n>0)?sumy/n:0;
      }
  for (k=0;k<mris->nvertices;k++)
    if ((!mris->vertices[k].ripflag)&&mris->vertices[k].border)
      {
  r = sqrt(SQR(mris->vertices[k].bnx)+SQR(mris->vertices[k].bny));
  if (r>0)
    {
      mris->vertices[k].bnx /= r;
      mris->vertices[k].bny /= r;
    }
      }
#endif
}

void
subsample_dist(int spacing)
{
  int k ;
  
  sub_num = MRISsubsampleDist(mris, spacing) ;
  MRISclearMarks(mris) ;
  for (k = 0 ; k < mris->nvertices ; k++)
    if (mris->vertices[k].d == 0)
      mris->vertices[k].marked = 1 ;
  
  printf("surfer: surface distance subsampled to %d vertices\n",sub_num);PR
                     }

/* 1-adjdot */
void
subsample_orient(float spacing)
{
  int k,m,n;
  VERTEX *v,*v2,*v3;
  float d;
  
  sub_num = 0;
  for (k=0;k<mris->nvertices;k++)
    {
      v = &mris->vertices[k];
      v->d = 10000;
      v->val = 0;
    }
  for (k=0;k<mris->nvertices;k++)
    if (mris->vertices[k].d > 0)
      {
  v = &mris->vertices[k];
  for (m=0;m<v->vnum;m++)
    {
      v2 = &mris->vertices[v->v[m]];
      if (v2->d == 0)
        {
    d = 1.0 - (v->nx*v2->nx+v->ny*v2->ny+v->nz*v2->nz);
    if (v->d > d) v->d = d;
        }
      for (n=0;n<v2->vnum;n++)
        {
    v3 = &mris->vertices[v2->v[n]];
    if (v3->d == 0)
      {
        d = 1.0 - (v->nx*v3->nx+v->ny*v3->ny+v->nz*v3->nz);
        if (v->d > d) v->d = d;
      }
        }
    }
  if (v->d>=spacing)
    {
      v->d = 0;
      v->val = 1;
      v->fixedval = TRUE;
      sub_num++;
    }
      }
  for (k=0;k<mris->nvertices;k++)
    if (mris->vertices[k].d == 0)
      {
  v = &mris->vertices[k];
  n = 0;
  for (m=0;m<v->vnum;m++)
    {
      if (mris->vertices[v->v[m]].d > 0)
        n++;
    }
  if (n == 0)
    {
      v->d = 1;
      v->val = 0;
      sub_num--;
    }
      }
  
  printf("surfer: surface orientation subsampled to %d vertices\n",sub_num);PR
                        }

void
smooth_curv(int niter)
{
  int iter,k,m,n;
  VERTEX *v;
  float sum;
  
  for (iter=0;iter<niter;iter++)
    {
      for (k=0;k<mris->nvertices;k++)
  {
    v = &mris->vertices[k];
    sum = v->curv;
    n = 1;
    for (m=0;m<v->vnum;m++)
      if (!mris->vertices[v->v[m]].ripflag)
        {
    if (mris->vertices[v->v[m]].d <= v->d)
      {
        sum += mris->vertices[v->v[m]].curv;
        n++;
      }
        }
    if (n>0) v->curv = sum/n;
  }
    }
}

#if 1
void
smooth_val_sparse(int niter)
{
  int    iter,k,m,n;
  VERTEX *v;
  float  sum;
  
  printf("surfer: smooth_val_sparse(%d)\n",niter);
  for (iter=0;iter<niter;iter++)
    {
      printf(".");fflush(stdout);
      for (k=0;k<mris->nvertices;k++)
  if (!mris->vertices[k].ripflag)
    {
      mris->vertices[k].tdx = mris->vertices[k].val;
      mris->vertices[k].old_undefval = mris->vertices[k].undefval;
    }
      for (k=0;k<mris->nvertices;k++)
  if (!mris->vertices[k].ripflag)
    if (!mris->vertices[k].fixedval)
      {
        v = &mris->vertices[k];
        if (k == 3)
    DiagBreak() ;
        sum=0;
        n = 0;
        if (!v->old_undefval)
    {
      sum = v->val;
      n = 1;
    }
        for (m=0;m<v->vnum;m++)
    {
      /*        if (mris->vertices[v->v[m]].dist < v->dist) */
      /*        if (mris->vertices[v->v[m]].dist <= v->dist) */
      if (!mris->vertices[v->v[m]].old_undefval)
        {
          sum += mris->vertices[v->v[m]].tdx;
          n++;
        }
    }
        if (n>0) 
    {
      v->val = sum/n;
      v->undefval = FALSE;
    }
      }
    }
  printf("\n");PR
     }
#else
void
smooth_val_sparse(int niter)
{
#if 0
  int    vno ;
  VERTEX *v ;
  
  for (vno = 0 ; vno < mris->nvertices ; vno++)
    {
      v = &mris->vertices[vno] ;
      v->marked = v->fixedval ;
    }
  MRISsoapBubbleVals(mris, niter) ;
  MRISclearMarks(mris) ;
  
#else
  int iter,k,m,n;
  VERTEX *v;
  float sum;
  
  printf("surfer: smooth_val_sparse(%d)\n",niter);
  for (iter=0;iter<niter;iter++)
    {
      printf(".");fflush(stdout);
      for (k=0;k<mris->nvertices;k++)
  if (!mris->vertices[k].ripflag)
    {
      mris->vertices[k].tdx = mris->vertices[k].val;
      mris->vertices[k].old_undefval = mris->vertices[k].undefval;
    }
      for (k=0;k<mris->nvertices;k++)
  if (!mris->vertices[k].ripflag)
    if (!mris->vertices[k].fixedval)
      {
        v = &mris->vertices[k];
        sum=0;
        n = 0;
        if (!v->old_undefval)
    {
      sum = v->val;
      n = 1;
    }
        for (m=0;m<v->vnum;m++)
    {
      /*        if (mris->vertices[v->v[m]].d < v->d) */
      /*        if (mris->vertices[v->v[m]].d <= v->d) */
      if (!mris->vertices[v->v[m]].old_undefval)
        {
          sum += mris->vertices[v->v[m]].tdx;
          n++;
        }
    }
        if (n>0) 
    {
      v->val = sum/n;
      v->undefval = FALSE;
    }
      }
    }
  printf("\n");PR
#endif
     }
#endif

void
smooth_val(int niter)
{
  int iter,k,m,n;
  VERTEX *v;
  float sum;
  
  surface_compiled = 0 ;
  printf("surfer: smooth_val(%d)\n",niter);
  for (iter=0;iter<niter;iter++)
    {
      printf(".");fflush(stdout);
      for (k=0;k<mris->nvertices;k++)
  mris->vertices[k].tdx = mris->vertices[k].val;
      for (k=0;k<mris->nvertices;k++)
  {
    v = &mris->vertices[k];
    sum=v->tdx;
    if (k == Gdiag_no)
      DiagBreak() ;
    n = 1;
    for (m=0;m<v->vnum;m++)
      {
        sum += mris->vertices[v->v[m]].tdx;
        n++;
      }
    if (!finite(sum))
      DiagBreak() ;
    if (n>0) 
      v->val = sum/n;
    if (!finite(v->val))
      DiagBreak() ;
  }
    }
  printf("\n");PR
     }

void
smooth_fs(int niter)
{
  int iter,k,m,n;
  VERTEX *v;
  float sum;
  
  printf("surfer: smooth_fs(%d)\n",niter);
  for (iter=0;iter<niter;iter++)
    {
      printf(".");fflush(stdout);
      for (k=0;k<mris->nvertices;k++)
  mris->vertices[k].tdx = mris->vertices[k].fieldsign;
      for (k=0;k<mris->nvertices;k++)
  {
    v = &mris->vertices[k];
    sum=v->tdx;
    n = 1;
    for (m=0;m<v->vnum;m++)
      {
        sum += mris->vertices[v->v[m]].tdx;
        n++;
      }
    if (n>0) v->fieldsign = sum/n;
  }
    }
  printf("\n");PR
     }

/* begin rkt */

int
sclv_smooth(int niter, int field)
{
  int iter,k,m,n;
  VERTEX *v;
  float sum;
  
  surface_compiled = 0 ;
  printf("surfer: sclv_smooth(%d,%s)\n",niter,sclv_field_names[field]);
  for (iter=0;iter<niter;iter++)
    {
      printf(".");fflush(stdout);
      for (k=0;k<mris->nvertices;k++)
  sclv_get_value( (&(mris->vertices[k])), field, &(mris->vertices[k].tdx));
      for (k=0;k<mris->nvertices;k++)
  {
    v = &mris->vertices[k];
    sum=v->tdx;
    if (k == Gdiag_no)
      DiagBreak() ;
    n = 1;
    for (m=0;m<v->vnum;m++)
      {
        sum += mris->vertices[v->v[m]].tdx;
        n++;
      }
    if (!finite(sum))
      DiagBreak() ;
    if (n>0) 
      sclv_set_value (v, field, sum/n);
    if (!finite(sum/n))
      DiagBreak() ;
  }
    }
  printf("\n");PR;
  
  return (ERROR_NONE);
}

/* end rkt */

void
fatten_border(int niter)
{
  int iter,k,m,n;
  VERTEX *v;
  
  printf("surfer: fatten_border(%d)\n",niter);
  n = 0;
  for (iter=0;iter<niter;iter++)
    {
      for (k=0;k<mris->nvertices;k++)
  mris->vertices[k].d = mris->vertices[k].border;
      for (k=0;k<mris->nvertices;k++)
  {
    v = &mris->vertices[k];
    for (m=0;m<v->vnum;m++)
      if (mris->vertices[v->v[m]].d!=0)
        {
    v->border = TRUE;
    n++;
        }
  }
    }
  printf("surfer: %d border points added\n",n);PR
             }

void
compute_angles(void)
{
  int k;
  float val,valbak,val2,val2bak;
  
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag)
      {
  val = atan2(mris->vertices[k].val2,mris->vertices[k].val);
  val2 = sqrt(SQR(mris->vertices[k].val2)+SQR(mris->vertices[k].val));
  valbak = atan2(mris->vertices[k].val2bak,mris->vertices[k].valbak);
  val2bak = sqrt(SQR(mris->vertices[k].val2bak)+SQR(mris->vertices[k].valbak));
  mris->vertices[k].val = val;
  mris->vertices[k].val2 = val2;
  mris->vertices[k].valbak = valbak;
  mris->vertices[k].val2bak = val2bak;
      } 
}

float circsubtract(float a,float b)
{
  float h = a-b;
  if (h<-M_PI) h = h+2*M_PI;
  else if (h>M_PI) h = h-2*M_PI;
  return h;
}

void
compute_fieldsign(void)
{
  int k,m,n;
  VERTEX *v;
  float dv1,dv2,dx,dy,dv1dx,dv1dy,dv2dx,dv2dy;
  float m11,m12,m13,m22,m23,z1,z2,z3,z1b,z2b,z3b,denom;
  
  MRISremoveTriangleLinks(mris) ;
  printf("surfer: compute_fieldsign()\n");PR
              for (k=0;k<mris->nvertices;k++)
                if (!mris->vertices[k].ripflag)
            {
              v = &mris->vertices[k];
              dv1dx = dv1dy = dv2dx = dv2dy = 0;
              m11 = m12 = m13 = m22 = m23 = z1 = z2 = z3 = z1b = z2b = z3b = 0;
              n = 0;
              for (m=0;m<v->vnum;m++)
                if (!mris->vertices[v->v[m]].ripflag)
                  {
              dv1 = circsubtract(v->val,mris->vertices[v->v[m]].val);
              dv2 = circsubtract(v->valbak,mris->vertices[v->v[m]].valbak);
              dx = v->x-mris->vertices[v->v[m]].x;
              dy = v->y-mris->vertices[v->v[m]].y;
              m11 += dx*dx;
              m12 += dx*dy;
              m13 += dx;
              m22 += dy*dy;
              m23 += dy;
              z1 += dx*dv1;
              z2 += dy*dv1;
              z3 += dv1;
              z1b += dx*dv2;
              z2b += dy*dv2;
              z3b += dv2;
                  }
              dv1dx = (m22*z1-m23*m23*z1-m12*z2+m13*m23*z2-m13*m22*z3+m12*m23*z3);
              dv2dx = (m22*z1b-m23*m23*z1b-m12*z2b+m13*m23*z2b-m13*m22*z3b+m12*m23*z3b);
              dv1dy = (-m12*z1+m13*m23*z1+m11*z2-m13*m13*z2+m12*m13*z3-m11*m23*z3);
              dv2dy = (-m12*z1b+m13*m23*z1b+m11*z2b-m13*m13*z2b+m12*m13*z3b-m11*m23*z3b);
              denom = -m12*m12+m11*m22-m13*m13*m22+2*m12*m13*m23-m11*m23*m23;
              if (denom!=0)
                v->fieldsign = (dv1dx*dv2dy-dv2dx*dv1dy)/(denom*denom);
              else
                v->fieldsign = 0;
              
              v->fieldsign =  ((v->fieldsign<0)?-1:(v->fieldsign>0)?1:0);
              if (revfsflag)
                v->fieldsign = -(v->fieldsign);
              
              v->fsmask = sqrt(v->val2*v->val2bak);  /* geom mean of r,th power */
            }
  fieldsignflag = TRUE;
}

void
compute_cortical_thickness(void)   /* marty */
{
  VERTEX *v;
  int imnr,i,j;
  int h,k,m,n;
  int outi,outj,outim;
  int gmval;
  float sum;
  float gmvalhist[GRAYBINS];
  float gmdist;
  float x,y,z;
  
  if (!MRIflag || !MRIloaded) {
    printf("surfer: ### need MRI data to compute cortical thickness\n");PR return;}
  
  compute_normals();
  
  for (k=0;k<mris->nvertices;k++)
    {
      v = &mris->vertices[k];
      v->d = 1;   /* for smooth_val_sparse */
      v->val = 0.0;
      x = v->x;
      y = v->y;
      z = v->z;
      for (h=0;h<GRAYBINS;h++)
  {
    gmdist = (float)h*GRAYINCR;
    
    imnr = (int)((y-yy0)/st+0.5-imnr0);
    i = (int)((zz1-z)/ps+0.5);
    j = (int)((xx1-x)/ps+0.5);
    imnr = (imnr<0)?0:(imnr>=numimg)?numimg-1:imnr;
    i = (i<0)?0:(i>=IMGSIZE)?IMGSIZE-1:i;
    j = (j<0)?0:(j>=IMGSIZE)?IMGSIZE-1:j;
    outim = (int)(imnr+gmdist/st*v->ny+0.5);
    outi = (int)(i-gmdist/ps*v->nz+0.5);
    outj = (int)(j-gmdist/ps*v->nx+0.5);
    if (outim>=0&&outim<numimg) gmval = im[outim][outi][outj];
    else                        gmval = 0;
    
    sum = gmval;
    n = 1;
    
    for (m=0;m<v->vnum;m++)
      {
        x = mris->vertices[v->v[m]].x;
        y = mris->vertices[v->v[m]].y;
        z = mris->vertices[v->v[m]].z;
        imnr = (int)((y-yy0)/st+0.5-imnr0);
        i = (int)((zz1-z)/ps+0.5);
        j = (int)((xx1-x)/ps+0.5);
        imnr = (imnr<0)?0:(imnr>=numimg)?numimg-1:imnr;
        i = (i<0)?0:(i>=IMGSIZE)?IMGSIZE-1:i;
        j = (j<0)?0:(j>=IMGSIZE)?IMGSIZE-1:j;
        outim = (int)(imnr+gmdist/st*v->ny+0.5);
        outi = (int)(i-gmdist/ps*v->nz+0.5);
        outj = (int)(j-gmdist/ps*v->nx+0.5);
        if (outim>=0&&outim<numimg) gmval = im[outim][outi][outj];
        else                        gmval = 0;
        
        sum += gmval;
        n++;
      }
    if (n>0) gmvalhist[h] = sum/n;
    else     gmvalhist[h] = 0.0;
    /*** printf("avggmval[%d]=%f ",h,gmvalhist[h]); */
  }
      for (h=0;h<GRAYBINS;h++)
  {
    gmdist = (float)h*GRAYINCR;
    if (gmvalhist[h]<=mingm)
      {
        gmdist -= 0.5 * (GRAYBINS-1)*GRAYINCR; /* green thin, red thick */
        v->val = gmdist;  /* no gm dropoff: val=0.0 -  */
        break;
      }
  }
      /*** printf("gmdist=%f\n\n",gmdist); */
    }
}

void
compute_CMF(void)  /* TODO: update to use new labels! */
{
  int i,j,k,m,n,label,bin;
  VERTEX *v;
  float dv1,dv2,dx,dy,dv1dx,dv1dy,dv2dx,dv2dy;
  float m11,m12,m13,m22,m23,z1,z2,z3,z1b,z2b,z3b,denom;
  float sum,a,val1,val2,rad,radmax = 0.0f;
  char fname[200];
  FILE *fp;
  float maxrad=10.0,minrad=0.05*maxrad,afact;
  
  afact = pow(maxrad/minrad,1.0/(maxrad-minrad));
  for (i=0;i<NLABELS;i++)
    {
      valtot[i] = 0;
      radmin[i] = 1000000;
      gradx_avg[i] = 0;
      grady_avg[i] = 0;
      for (j=0;j<CMFBINS;j++)
  {
    val1avg[i][j] = 0;
    val2avg[i][j] = 0;
    valnum[i][j] = 0;
  }
    }
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag && !mris->vertices[k].border)
      {
  v = &mris->vertices[k];
  a = v->val/(2*M_PI);
  a += 0.5;
  a += angle_offset;
  a = fmod(a,1.0);
  v->val = a*2*M_PI;
      }
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag && !mris->vertices[k].border)
      if (mris->vertices[k].annotation!=0)
  {
    v = &mris->vertices[k];
    dv1dx = dv1dy = dv2dx = dv2dy = 0;
    m11 = m12 = m13 = m22 = m23 = z1 = z2 = z3 = z1b = z2b = z3b = 0;
    n = 0;
    for (m=0;m<v->vnum;m++)
      if (!mris->vertices[v->v[m]].ripflag && !mris->vertices[v->v[m]].border)
        {
    dv1 = circsubtract(v->val,mris->vertices[v->v[m]].val);
    dv2 = circsubtract(v->valbak,mris->vertices[v->v[m]].valbak);
    dx = v->x-mris->vertices[v->v[m]].x;
    dy = v->y-mris->vertices[v->v[m]].y;
    m11 += dx*dx;
    m12 += dx*dy;
    m13 += dx;
    m22 += dy*dy;
    m23 += dy;
    z1 += dx*dv1;
    z2 += dy*dv1;
    z3 += dv1;
    z1b += dx*dv2;
    z2b += dy*dv2;
    z3b += dv2;
        }
    dv1dx = (m22*z1-m23*m23*z1-m12*z2+m13*m23*z2-m13*m22*z3+m12*m23*z3);
    dv2dx = (m22*z1b-m23*m23*z1b-m12*z2b+m13*m23*z2b-m13*m22*z3b+m12*m23*z3b);
    dv1dy = (-m12*z1+m13*m23*z1+m11*z2-m13*m13*z2+m12*m13*z3-m11*m23*z3);
    dv2dy = (-m12*z1b+m13*m23*z1b+m11*z2b-m13*m13*z2b+m12*m13*z3b-m11*m23*z3b);
    denom = -m12*m12+m11*m22-m13*m13*m22+2*m12*m13*m23-m11*m23*m23;
    if (denom!=0)
      {
        dv1dx /= denom;
        dv2dx /= denom;
        dv1dy /= denom;
        dv2dy /= denom;
      }
    label = mris->vertices[k].annotation;
    gradx_avg[label] += dv1dx;
    grady_avg[label] += dv1dy;
  }
  for (i=0;i<NLABELS;i++)
    {
      a = sqrt(SQR(gradx_avg[i])+SQR(grady_avg[i]));
      if (a!=0)
  {
    gradx_avg[i] /= a;
    grady_avg[i] /= a;
  }
    }
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag && !mris->vertices[k].border)
      if (mris->vertices[k].annotation!=0)
  {
    v = &mris->vertices[k];
    label = mris->vertices[k].annotation;
    rad = (v->x*gradx_avg[label]+v->y*grady_avg[label]);
    if (rad<radmin[label]) radmin[label] = rad;
  }
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag && !mris->vertices[k].border)
      if (mris->vertices[k].annotation!=0)
  {
    v = &mris->vertices[k];
    label = mris->vertices[k].annotation;
    rad = (v->x*gradx_avg[label]+v->y*grady_avg[label])-radmin[label];
    bin = floor(rad/MAXVAL*CMFBINS);
    if (bin<0)
      {printf("surfer: --- bin=%d too low\n",bin);bin=0;}
    if (bin>CMFBINS-1) 
      {printf("surfer: --- bin=%d too high\n",bin);bin=CMFBINS-1;}
    val1 = cos(v->val)*v->val2;
    val2 = sin(v->val)*v->val2;
    val1avg[label][bin] += val1;
    val2avg[label][bin] += val2;
    valnum[label][bin] += 1;
    valtot[label] += 1;
  }
  for (i=0;i<NLABELS;i++)
    if (valtot[i]!=0)
      {
  printf("surfer: label=%d\n",i);
  sum = 0;
  for (j=0;j<CMFBINS;j++)
    {
      if (valnum[i][j]!=0)
        {
    a = atan2(val2avg[i][j],val1avg[i][j]);
    if (a<0) a += 2*M_PI;
    a = 2*M_PI-a;
    a = a*maxrad/(2*M_PI);
    a = minrad*pow(afact,a-minrad);
    angleavg[i][j] = a;
        }
    }
  sprintf(fname,"cmftmp-%d.xplot",i);
  fp = fopen(fname,"w");
  for (j=0;j<CMFBINS;j++)
    if (valnum[i][j]!=0)
      radmax = j*MAXVAL/CMFBINS;
  for (j=0;j<CMFBINS;j++)
    if (valnum[i][j]!=0)
      {
        fprintf(fp,"%f %f\n",radmax-j*MAXVAL/CMFBINS,angleavg[i][j]);
      }
  fclose(fp);
      }
  PR
    }

void
smooth_momentum(int niter)
{
#if 0
  int iter,k,m,n;
  VERTEX *v;
  float sumx,sumy,sumz;
  
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag)
      {
  v = &mris->vertices[k];
  v->smx = v->odx;
  v->smy = v->ody;
  v->smz = v->odz;
      }
  for (iter=0;iter<niter;iter++)
    {
      for (k=0;k<mris->nvertices;k++)
  if (!mris->vertices[k].ripflag)
    {
      v = &mris->vertices[k];
      v->osmx = v->smx;
      v->osmy = v->smy;
      v->osmz = v->smz;
    }
      for (k=0;k<mris->nvertices;k++)
  if (!mris->vertices[k].ripflag)
    {
      v = &mris->vertices[k];
      sumx = v->osmx;
      sumy = v->osmy;
      sumz = v->osmz;
      n = 1;
      for (m=0;m<v->vnum;m++)
        if (!mris->vertices[v->v[m]].ripflag)
    {
      sumx += mris->vertices[v->v[m]].osmx;
      sumy += mris->vertices[v->v[m]].osmy;
      sumz += mris->vertices[v->v[m]].osmz;
      n++;
    }
      if (n>0) v->smx = sumx/n;
      if (n>0) v->smy = sumy/n;
      if (n>0) v->smz = sumz/n;
    }
    }
#endif
}

void
smooth_logarat(int niter)
{
#if 0
  int iter,k,m,n;
  VERTEX *v;
  float sum,arat;
  
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag)
      {
  arat = mris->vertices[k].area/mris->vertices[k].origarea;
  mris->vertices[k].logarat = (arat>0)?log(arat):0;
  /*
    printf("%d: area=%f, origarea=%f, arat=%f, logarat=%f\n",
    k,mris->vertices[k].area,mris->vertices[k].origarea,arat,mris->vertices[k].logarat);
  */
      }
  for (iter=0;iter<niter;iter++)
    {
      for (k=0;k<mris->nvertices;k++)
  if (!mris->vertices[k].ripflag)
    mris->vertices[k].ologarat = mris->vertices[k].logarat;
      for (k=0;k<mris->nvertices;k++)
  if (!mris->vertices[k].ripflag)
    {
      v = &mris->vertices[k];
      sum = v->ologarat;
      n = 1;
      for (m=0;m<v->vnum;m++)
        if (!mris->vertices[v->v[m]].ripflag)
    {
      sum += mris->vertices[v->v[m]].ologarat;
      n++;
    }
      if (n>0) v->logarat = sum/n;
    }
    }
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag)
      {
  mris->vertices[k].sqrtarat = exp(0.5*mris->vertices[k].logarat);
      }
#endif
}

void
smooth_shear(int niter)
{
#if 0
  int iter,k,m,n;
  VERTEX *v;
  float sumx,sumy,r;
  
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag)
      {
  double_angle(mris->vertices[k].shearx,mris->vertices[k].sheary,
         &mris->vertices[k].shearx,&mris->vertices[k].sheary);
      }
  for (iter=0;iter<niter;iter++)
    {
      for (k=0;k<mris->nvertices;k++)
  if (!mris->vertices[k].ripflag)
    {
      mris->vertices[k].oshearx = mris->vertices[k].shearx;
      mris->vertices[k].osheary = mris->vertices[k].sheary;
    }
      for (k=0;k<mris->nvertices;k++)
  if (!mris->vertices[k].ripflag)
    {
      v = &mris->vertices[k];
      sumx = v->oshearx;
      sumy = v->osheary;
      n = 1;
      for (m=0;m<v->vnum;m++)
        if (!mris->vertices[v->v[m]].ripflag)
    {
      sumx += mris->vertices[v->v[m]].oshearx;
      sumy += mris->vertices[v->v[m]].osheary;
      n++;
    }
      v->shearx = (n>0)?sumx/n:0;
      v->sheary = (n>0)?sumy/n:0;
    }
    }
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag)
      {
  halve_angle(mris->vertices[k].shearx,mris->vertices[k].sheary,
        &mris->vertices[k].shearx,&mris->vertices[k].sheary);
  r = sqrt(SQR(mris->vertices[k].shearx)+SQR(mris->vertices[k].sheary));
  mris->vertices[k].logshear = r;
      }
#endif
}

void
smooth_boundary_normals(int niter)
{
#if 0
  int iter,k,m,n;
  VERTEX *v;
  float sumx,sumy,r;
  
  for (iter=0;iter<niter;iter++)
    {
      for (k=0;k<mris->nvertices;k++)
  if ((!mris->vertices[k].ripflag)&&mris->vertices[k].border)
    {
      mris->vertices[k].obnx = mris->vertices[k].bnx;
      mris->vertices[k].obny = mris->vertices[k].bny;
    }
      for (k=0;k<mris->nvertices;k++)
  if ((!mris->vertices[k].ripflag)&&mris->vertices[k].border)
    {
      v = &mris->vertices[k];
      n = 1;
      sumx = v->obnx;
      sumy = v->obny;
      for (m=0;m<v->vnum;m++)
        if ((!mris->vertices[v->v[m]].ripflag)&&mris->vertices[v->v[m]].border)
    {
      sumx += mris->vertices[v->v[m]].obnx;
      sumy += mris->vertices[v->v[m]].obny;
      n++;
    }
      v->bnx = (n>0)?sumx/n:0;
      v->bny = (n>0)?sumy/n:0;
    }
    }
  for (k=0;k<mris->nvertices;k++)
    if ((!mris->vertices[k].ripflag)&&mris->vertices[k].border)
      {
  r = sqrt(SQR(mris->vertices[k].bnx)+SQR(mris->vertices[k].bny));
  if (r>0)
    {
      mris->vertices[k].bnx /= r;
      mris->vertices[k].bny /= r;
    }
      }
#endif
}

void
scaledist(float sf)
{
  int k;
  VERTEX *v;
  
  printf("surfer: scaledist(%f)\n",sf);PR
           for (k=0;k<mris->nvertices;k++)
             {
               v = &mris->vertices[k];
               v->x = (v->x+mris->xctr)*sf-mris->xctr;
               v->y = (v->y-mris->yctr)*sf+mris->yctr;
               v->z = (v->z-mris->zctr)*sf+mris->zctr;
             }
}

float rtanh(float x)
{
  return (x<0.0)?0.0:tanh(x);
}

void
shrink(int niter)   /* shrinktypes: nei,mri,curv,exp,area,2d,fill,sphe,ell */
{
  float x,y,z,sx,sy,sz,val,inval,outval,nc,force;
  float d,dx,dy,dz,sval,sinval,soutval,snc,anc, orig_area, scale ;
  float nx,ny,nz;
  VERTEX *v;
  int imnr,i,j,iter,k,m,n;
  float stress,sd,ad,dmax;
  int navg,an,nclip,inim,ini,inj,outim,outi,outj;
  
  orig_area = mris->total_area ;
  val = inval = outval = 0.0f ;
  if (shrinkfillflag) {shrink_to_fill(niter);return;}
  if (flag2d) {shrink2d(niter);return;}
  if (areaflag) {area_shrink(niter);return;}
  if (MRIflag && !MRIloaded)
    printf("surfer: MRIflag on but no MRI data loaded... MRIflag ignored\n");
  
  for (iter=0;iter<niter;iter++)
    {
      ad = 0;
      dmax = 0;
      an = 0;
      nclip = 0;
      for (k=0;k<mris->nvertices;k++)
  {
    v = &mris->vertices[k];
    v->tx = v->x;
    v->ty = v->y;
    v->tz = v->z;
    /*      v->onc = v->nc;*/
    v->oripflag = v->ripflag;
  }
      sval = sinval = soutval = snc = 0;
      maxstress = avgstress = 0;
      navg = 0;
      for (k=0;k<mris->nvertices;k++)
  if (!mris->vertices[k].oripflag)
    {
      v = &mris->vertices[k];
      x = v->tx;
      y = v->ty;
      z = v->tz;
      nx = v->nx;
      ny = v->ny;
      nz = v->nz;
      sx=sy=sz=sd=0;
      n=0;
      for (m=0;m<v->vnum;m++)
        if (!mris->vertices[v->v[m]].oripflag)
    {
      sx += dx = mris->vertices[v->v[m]].tx - x;
      sy += dy = mris->vertices[v->v[m]].ty - y;
      sz += dz = mris->vertices[v->v[m]].tz - z;
      sd += sqrt(dx*dx+dy*dy+dz*dz);
      n++;
    }
      if (n>0)
        {
    sx = sx/n;
    sy = sy/n;
    sz = sz/n;
    sd = sd/n;
    stress = sd;
    if (explodeflag && stress>=stressthresh)
      {
        nrip++;
        v->ripflag = TRUE;
      }
    if (stress>maxstress)
      maxstress = stress;
    avgstress += stress;
    navg++;
#if 0
    v->stress = stress;
#endif
        }
      if (MRIflag && MRIloaded)
        {
    imnr = (int)((y-yy0)/st+0.5);
    i = (int)((zz1-z)/ps+0.5);
    j = (int)((xx1-x)/ps+0.5);
    imnr = (imnr<0)?0:(imnr>=numimg)?numimg-1:imnr;
    i = (i<0)?0:(i>=IMGSIZE)?IMGSIZE-1:i;
    j = (j<0)?0:(j>=IMGSIZE)?IMGSIZE-1:j;
    val = im[imnr][i][j];
    inim = (int)(imnr-cthk/st*v->ny+0.5);
    ini = (int)(i+cthk/ps*v->nz+0.5);
    inj = (int)(j+cthk/ps*v->nx+0.5);
    if (inim>=0&&inim<numimg)
      inval = im[inim][ini][inj];
    else
      inval = 0;
    outim = (int)(imnr+ostilt/st*v->ny+0.5);
    outi = (int)(i-ostilt/ps*v->nz+0.5);
    outj = (int)(j-ostilt/ps*v->nx+0.5);
    if (outim>=0&&outim<numimg)
      outval = im[outim][outi][outj];
    else
      outval = 0;
    /*      force = mstrength*tanh((val-mmid)*mslope); */
    /*      force = mstrength*tanh((inval-mmid)*mslope); */
    /*
      force = mstrength*(tanh((inval-mmid)*mslope)
      -rtanh((outval-mmid)*mslope));
    */
    
    force = (mstrength*tanh((inval-whitemid)*mslope)+
       mstrength*tanh((val-graymid)*mslope))/2;
    
        } else
    if (expandflag)
      {
        force = 0.1;
      } else
        {
          force = 0;
        }
      nc = sx*nx+sy*ny+sz*nz;
      sx -= nc*nx;
      sy -= nc*ny;
      sz -= nc*nz;
      snc += nc;
      v->nc = nc;
#if 0
      if (stiffnessflag)
        {
    anc = 0;
    for (m=0;m<v->vnum;m++)
      anc += mris->vertices[v->v[m]].onc;
    anc /= v->vnum;
    force += tanh((nc-anc)*0.2);
        }
      else
#endif
        {
    anc = 0;
    force += tanh((nc-anc)*0.5);
        }
      dx = sx*0.5 + v->nx*force;
      dy = sy*0.5 + v->ny*force;
      dz = sz*0.5 + v->nz*force;
      if (momentumflag)
        {
    dx = decay*v->odx+update*dx;
    dy = decay*v->ody+update*dy;
    dz = decay*v->odz+update*dz;
    if ((d=sqrt(dx*dx+dy*dy+dz*dz))>1.0)
      {
        nclip ++;
        dx /= d;
        dy /= d;
        dz /= d;
      }
    v->odx = dx;
    v->ody = dy;
    v->odz = dz;
        }
      if (sulcflag)
        v->curv += dx*v->nx + dy*v->ny + dz*v->nz;
      d=sqrt(dx*dx+dy*dy+dz*dz);
      if (d>dmax) dmax = d;
      ad += d;
      an ++;
      v->x += dx;
      v->y += dy;
      v->z += dz;
      sval += val;
      sinval += inval;
      soutval += outval;
    }
      snc /= mris->nvertices;
      if (MRIflag && MRIloaded)
  {
    compute_normals();
    sval /= mris->nvertices;
    sinval /= mris->nvertices;
    soutval /= mris->nvertices;
    printf("surfer: %d: sval=%5.2f,sinval=%5.2f,soutval=%5.2f,snc=%5.2f\n",
     iter,sval,sinval,soutval,snc);
  } else
    {
      printf("surfer: %d: ad=%f, dmax=%f, snc=%f\n",iter,ad/an,dmax,snc);
      if (expandflag || explodeflag)
        compute_normals();
    }
      if (navg>0)
  avgstress /= navg;
      if (explodeflag)
  printf("surfer: %d: max = %f, avg = %f, threshold = %f, nrip = %d\n",
         iter,maxstress,avgstress,stressthresh,nrip);
      PR
  }
  if (!(MRIflag || expandflag || explodeflag))
    {
      compute_normals();
    }
  
  MRIScomputeMetricProperties(mris) ;
  scale = sqrt(orig_area / (mris->total_area+mris->neg_area)) ;
  MRISscaleBrain(mris, mris, scale) ;
  MRIScomputeMetricProperties(mris) ;
  vset_save_surface_vertices(vset_current_set) ;
  vset_set_current_set(vset_current_set) ;
}

void
curv_shrink_to_fill(int niter)
{
  VERTEX *v;
  int imnr,i,j,iter,k,m,n;
  int an,nclip,delcurvdefined;
  float x,y,z,sx,sy,sz,nc,force;
  float d,dx,dy,dz,snc;
  float nx,ny,nz;
  float val,sval;
  float sd,ad,dmax;
  float xnei,ynei,znei;
  float curv,icurv,icurvnei,cfact;
  float icrange,crange;
  
  icurv = curv = icurvnei = 0.0f ;
  if (!curvloaded)   { printf("surfer: ### curv not loaded!\n"); PR return; }
  if (!curvimloaded) { printf("surfer: ### curvim not loaded!\n"); PR return; }
  if (!curvimflag)   { printf("surfer: ### curvimflag not set!\n"); PR return; }
  
  icrange = mris2->max_curv-mris2->min_curv;
  crange = mris->max_curv-mris->min_curv;
  for (iter=0;iter<niter;iter++) {
    ad = 0;
    dmax = 0;
    an = 0;
    nclip = 0;
    for (k=0;k<mris->nvertices;k++) {
      v = &mris->vertices[k];
      v->tx = v->x;
      v->ty = v->y;
      v->tz = v->z;
      /*      v->onc = v->nc;*/
      v->oripflag = v->ripflag;
    }
    sval = snc = 0;
    for (k=0;k<mris->nvertices;k++)
      if (!mris->vertices[k].oripflag) {
  v = &mris->vertices[k];
  x = v->tx;
  y = v->ty;
  z = v->tz;
  nx = v->nx;
  ny = v->ny;
  nz = v->nz;
  sx=sy=sz=sd=0;
  n=0;
  
  /**** nei curve force */
  delcurvdefined = TRUE;
  imnr = (int)((y-yy0)/st+0.5);
  i = (int)((zz1-z)/ps+0.5);
  j = (int)((xx1-x)/ps+0.5);
  if (curvimflags[imnr][i][j] & CURVIM_DEFINED)
    icurv = bytetofloat(curvim[imnr][i][j],mris2->min_curv,mris2->max_curv);
  else
    delcurvdefined = FALSE;
  curv = v->curv;
  for (m=0;m<v->vnum;m++)
    if (!mris->vertices[v->v[m]].oripflag) {
      xnei = mris->vertices[v->v[m]].tx;
      ynei = mris->vertices[v->v[m]].ty;
      znei = mris->vertices[v->v[m]].tz;
      imnr = (int)((ynei-yy0)/st+0.5);
      i = (int)((zz1-znei)/ps+0.5);
      j = (int)((xx1-xnei)/ps+0.5);
      if (curvimflags[imnr][i][j] & CURVIM_DEFINED)
        icurvnei = bytetofloat(curvim[imnr][i][j],mris2->min_curv,mris2->max_curv);
      else
        delcurvdefined = FALSE;
      cfact = 1.0;
      if (delcurvdefined)
        cfact += (icurvnei-icurv)/icrange
    * copysign(icstrength,mris2->min_curv+curv*(icrange/crange) - icurv);
      sx += dx = (xnei - x)*cfact;
      sy += dy = (ynei - y)*cfact;
      sz += dz = (znei - z)*cfact;
      sd += sqrt(dx*dx+dy*dy+dz*dz);
      n++;
    }
  if (n>0) {
    sx = sx/n;
    sy = sy/n;
    sz = sz/n;
    sd = sd/n;
  }
  
  /**** norm shrink_to_fill force */
  imnr = (int)(y+numimg/2.0+0.5);
  i = (int)(IMGSIZE/2.0-z+0.5);
  j = (int)(IMGSIZE/2.0-x+0.5);
  imnr = (imnr<0)?0:(imnr>=numimg)?numimg-1:imnr;
  i = (i<0)?0:(i>=IMGSIZE)?IMGSIZE-1:i;
  j = (j<0)?0:(j>=IMGSIZE)?IMGSIZE-1:j;
  val = (curvimflags[imnr][i][j] & CURVIM_FILLED)?128:0;
  force = mstrength*tanh((val-mmid)*mslope);
  nc = sx*nx+sy*ny+sz*nz;
  snc += nc;
  v->nc = nc;
  force += tanh(nc*0.5);
  
  dx = sx*0.5 + v->nx*force;
  dy = sy*0.5 + v->ny*force;
  dz = sz*0.5 + v->nz*force;
  if (momentumflag) {
    dx = decay*v->odx+update*dx;
    dy = decay*v->ody+update*dy;
    dz = decay*v->odz+update*dz;
    if ((d=sqrt(dx*dx+dy*dy+dz*dz))>1.0) {
      nclip ++;
      dx /= d;
      dy /= d;
      dz /= d;
    }
    v->odx = dx;
    v->ody = dy;
    v->odz = dz;
  }
  d=sqrt(dx*dx+dy*dy+dz*dz);
  if (d>dmax) dmax = d;
  ad += d;
  an ++;
  v->x += dx;
  v->y += dy;
  v->z += dz;
  sval += val;
      }
    compute_normals();  /* only inside in shrink_to_fill */
    snc /= mris->nvertices;
    printf("surfer: %d: sval=%5.2f,snc=%5.2f\n",iter,sval,snc);
    PR
      }
}

void
shrink_to_fill(int niter)
{
  float x,y,z,sx,sy,sz,val,inval,outval,nc,force;
  float d,dx,dy,dz,sval,sinval,soutval,snc;
  float nx,ny,nz;
  VERTEX *v;
  int imnr,i,j,iter,k,m,n;
  float sd,ad,dmax;
  int navg,an,nclip,inim,ini,inj,outim,outi,outj;
  
  outval = inval = val = 0.0f ;
  for (iter=0;iter<niter;iter++)
    {
      ad = 0;
      dmax = 0;
      an = 0;
      nclip = 0;
      for (k=0;k<mris->nvertices;k++)
  {
    v = &mris->vertices[k];
    v->tx = v->x;
    v->ty = v->y;
    v->tz = v->z;
    /*      v->onc = v->nc;*/
    v->oripflag = v->ripflag;
  }
      sval = sinval = soutval = snc = 0;
      navg = 0;
      for (k=0;k<mris->nvertices;k++)
  if (!mris->vertices[k].oripflag)
    {
      v = &mris->vertices[k];
      x = v->tx;
      y = v->ty;
      z = v->tz;
      nx = v->nx;
      ny = v->ny;
      nz = v->nz;
      sx=sy=sz=sd=0;
      n=0;
      for (m=0;m<v->vnum;m++)
        if (!mris->vertices[v->v[m]].oripflag)
    {
      sx += dx = mris->vertices[v->v[m]].tx - x;
      sy += dy = mris->vertices[v->v[m]].ty - y;
      sz += dz = mris->vertices[v->v[m]].tz - z;
      sd += sqrt(dx*dx+dy*dy+dz*dz);
      n++;
    }
      if (n>0)
        {
    sx = sx/n;
    sy = sy/n;
    sz = sz/n;
    sd = sd/n;
    navg++;
        }
      if (MRIflag && MRIloaded)
        {
    imnr = (int)(y+numimg/2.0+0.5);
    i = (int)(IMGSIZE/2.0-z+0.5);
    j = (int)(IMGSIZE/2.0-x+0.5);
    imnr = (imnr<0)?0:(imnr>=numimg)?numimg-1:imnr;
    i = (i<0)?0:(i>=IMGSIZE)?IMGSIZE-1:i;
    j = (j<0)?0:(j>=IMGSIZE)?IMGSIZE-1:j;
    val = im[imnr][i][j];
    inim = (int)(imnr-cthk*v->ny+0.5);
    ini = (int)(i+cthk*v->nz+0.5);
    inj = (int)(j+cthk*v->nx+0.5);
    if (inim>=0&&inim<numimg)
      inval = im[inim][ini][inj];
    else
      inval = 0;
    outim = (int)(imnr+ostilt*v->ny+0.5);
    outi = (int)(i-ostilt*v->nz+0.5);
    outj = (int)(j-ostilt*v->nx+0.5);
    if (outim>=0&&outim<numimg)
      outval = im[outim][outi][outj];
    else
      outval = 0;
    force = mstrength*tanh((val-mmid)*mslope);
        } else force = 0;
      nc = sx*nx+sy*ny+sz*nz;
      /*
        sx -= nc*nx;
        sy -= nc*ny;
        sz -= nc*nz;
      */
      snc += nc;
      v->nc = nc;
      /*
        if (stiffnessflag)
        {
        anc = 0;
        for (m=0;m<v->vnum;m++)
        anc += mris->vertices[v->v[m]].onc;
        anc /= v->vnum;
        force += tanh((nc-anc)*0.2);
        }
        else
        {
        anc = 0;
        force += tanh((nc-anc)*0.5);
        }
      */
      dx = sx*0.5 + v->nx*force;
      dy = sy*0.5 + v->ny*force;
      dz = sz*0.5 + v->nz*force;
      if (momentumflag)
        {
    dx = decay*v->odx+update*dx;
    dy = decay*v->ody+update*dy;
    dz = decay*v->odz+update*dz;
    if ((d=sqrt(dx*dx+dy*dy+dz*dz))>1.0)
      {
        nclip ++;
        dx /= d;
        dy /= d;
        dz /= d;
      }
    v->odx = dx;
    v->ody = dy;
    v->odz = dz;
        }
      d=sqrt(dx*dx+dy*dy+dz*dz);
      if (d>dmax) dmax = d;
      ad += d;
      an ++;
      v->x += dx;
      v->y += dy;
      v->z += dz;
      sval += val;
      sinval += inval;
      soutval += outval;
    }
      snc /= mris->nvertices;
      compute_normals();
      if (MRIflag && MRIloaded)
  {
    sval /= mris->nvertices;
    sinval /= mris->nvertices;
    soutval /= mris->nvertices;
    printf("surfer: %d: sval=%5.2f,sinval=%5.2f,soutval=%5.2f,snc=%5.2f\n",
     iter,sval,sinval,soutval,snc);
  } else
    {
      printf("surfer: %d: ad=%f, dmax=%f, snc=%f\n",iter,ad/an,dmax,snc);
    }
      PR
  }
}

void
transform(float *xptr, float *yptr, float *zptr, float nx, float ny, float nz,
          float d)  /* 2 vects ortho to summed normal */
{
  float x = *xptr, y = *yptr, z = *zptr;
  
  *zptr = nx*x + ny*y + nz*z;
  *yptr = -ny/d*x + nx/d*y;
  *xptr = nx*nz/d*x + ny*nz/d*y - d*z;
  /*
    printf("transform {%f,%f,%f} -> {%f,%f,%f}\n",
    x,y,z,*xptr,*yptr,*zptr);
  */
}

void
really_translate_brain(float x, float y, float z)
{
  VERTEX *v;
  int i,j,k;
  float curr[4][4], accum[4][4]; /* Matrix curr,accum; */
  
  for (i=0;i<4;i++)
    for (j=0;j<4;j++)
      curr[i][j] = idmat[i][j];
  curr[3][0] = x;
  curr[3][1] = y;
  curr[3][2] = z;
  for (i=0;i<4;i++)
    for (j=0;j<4;j++) {
      accum[i][j] = 0;
      for (k=0;k<4;k++)
  accum[i][j] += curr[i][k]*reallymat[k][j];
    }
  for (i=0;i<4;i++)
    for (j=0;j<4;j++)
      reallymat[i][j] = accum[i][j];
  print_real_transform_matrix();
  
  for (k=0;k<mris->nvertices;k++) {
    v = &mris->vertices[k];
    v->x += x;
    v->y += y;
    v->z += z;
  }
}

void
really_scale_brain(float x, float y, float z)
{
  VERTEX *v;
  int i,j,k;
  float curr[4][4], accum[4][4]; /* Matrix curr,accum; */
  
  for (i=0;i<4;i++)
    for (j=0;j<4;j++)
      curr[i][j] = idmat[i][j];
  curr[0][0] = x;
  curr[1][1] = y;
  curr[2][2] = z;
  for (i=0;i<4;i++)
    for (j=0;j<4;j++) {
      accum[i][j] = 0;
      for (k=0;k<4;k++)
  accum[i][j] += curr[i][k]*reallymat[k][j];
    }
  for (i=0;i<4;i++)
    for (j=0;j<4;j++)
      reallymat[i][j] = accum[i][j];
  print_real_transform_matrix();
  
  for (k=0;k<mris->nvertices;k++) {
    v = &mris->vertices[k];
    v->x *= x;
    v->y *= y;
    v->z *= z;
  }
}

int
MRIStransformBrain(MRI_SURFACE *mris, 
       float exx, float exy, float exz, 
       float eyx, float eyy, float eyz, 
       float ezx, float ezy, float ezz)
{
  int     vno ;
  VERTEX  *v ;
  float   x, y, z ;
  
  for (vno = 0 ; vno < mris->nvertices ; vno++)
    {
      v = &mris->vertices[vno] ;
      if (v->ripflag)
  continue ;
      x = v->x ; y = v->y ; z = v->z ;
      v->x = x*exx + y*exy + z*exz ;
      v->y = x*eyx + y*eyy + z*eyz ;
      v->z = x*ezx + y*ezy + z*ezz ;
    }
  
  return(NO_ERROR) ;
}
void
align_sphere(MRI_SURFACE *mris)
{
  int     vno1, vno2 ;
  VERTEX  *v1, *v2 ;
  double  ex[3], ey[3], ez[3], len, p2[3], dot, tmp[3] ;
  
  /*
    two points must be selected on the spherical surface. The first
    will specify the z-axis (the 1st pole of the sphere), and the second 
    the x-axis (the orientation of the vertical meridian.
  */
  if (nmarked != 2)
    fprintf(stderr, "must pick origin and alignment points (previous two)\n");
  else
    {
      vno1 = marked[nmarked-2] ; vno2 = marked[nmarked-1] ; 
      v1 = &mris->vertices[vno1] ; v2 = &mris->vertices[vno2] ; 
      ez[0] = v1->x ; ez[1] = v1->y ; ez[2] = v1->z ; 
      len = VLEN(ez) ; SCALAR_MUL(ez, 1.0f/len, ez) ;
      p2[0] = v2->x ; p2[1] = v2->y ; p2[2] = v2->z ; 
      dot = DOT(ez, p2) ;
      SCALAR_MUL(tmp, -dot, ez) ;   /* take ez component out of ex */
      ADD(ex, p2, tmp) ;
      len = VLEN(ex) ; SCALAR_MUL(ex, 1.0f/len, ex) ;
      CROSS(ey, ez, ex) ;
      len = VLEN(ey) ; SCALAR_MUL(ey, 1.0f/len, ey) ;
      dot = DOT(ez, ex) ;
      dot = DOT(ez, ey) ;
      dot = DOT(ey, ex) ;
      MRIStransformBrain(mris,
       ex[0],ex[1],ex[2],ey[0],ey[1],ey[2],ez[0],ez[1],ez[2]);
      MRIScomputeMetricProperties(mris) ;
      redraw() ;
      printf("e0: (%2.2f, %2.2f, %2.2f), e1: (%2.2f, %2.2f, %2.2f), e2: (%2.2f, %2.2f, %2.2f)\n",
       ex[0], ex[1], ex[2], ey[0], ey[1], ey[2], ez[0], ez[1], ez[2]) ; PR
                    }
}

void
really_rotate_brain(float a, char axis)
{
  VERTEX *v;
  float x,y,z;
  float cosa,sina;
  int i,j,k;
  float curr[4][4], accum[4][4]; /* Matrix curr,accum; */
  
  a = a*M_PI/180;
  cosa = cos(a);
  sina = sin(a);
  
  for (i=0;i<4;i++)
    for (j=0;j<4;j++)
      curr[i][j] = idmat[i][j];
  if (axis=='y') {
    curr[0][0] = curr[2][2] = cosa;
    curr[2][0] = -(curr[0][2] = sina);
  } 
  else if (axis=='x') {
    curr[1][1] = curr[2][2] = cosa;
    curr[1][2] = -(curr[2][1] = sina);
  } 
  else if (axis=='z') {
    curr[0][0] = curr[1][1] = cosa;
    curr[1][0] = -(curr[0][1] = sina);
  }
  else {
    printf("surfer: ### Illegal axis %c\n",axis);return;PR
                }
  for (i=0;i<4;i++)
    for (j=0;j<4;j++) {
      accum[i][j] = 0;
      for (k=0;k<4;k++)
  accum[i][j] += curr[i][k]*reallymat[k][j];
    }
  for (i=0;i<4;i++)
    for (j=0;j<4;j++)
      reallymat[i][j] = accum[i][j];
  print_real_transform_matrix();
  
  for (k=0;k<mris->nvertices;k++) {
    v = &mris->vertices[k];
    x = v->x;
    y = v->y;
    z = v->z;
    if (axis=='x') {
      v->y = y*cosa - z*sina;
      v->z = y*sina + z*cosa;
    }
    else if (axis=='y') {
      v->x = x*cosa + z*sina;
      v->z = -x*sina + z*cosa;
    }
    else if (axis=='z') {
      v->x = x*cosa - y*sina;
      v->y = x*sina + y*cosa;
    }
    else {
      printf("surfer: ### Illegal axis %c\n",axis);return;PR
                  }
  }
}

void
really_align_brain(void)  /* trans cent first -> cent targ */
{
  VERTEX *v;
  VERTEX *v2;
  int k;
  float cx,cy,cz;
  float tcx,tcy,tcz;
  
  if (!secondsurfaceloaded) {
    printf("surfer: ### really_align brain failed: no second surface read\n");PR
                    return; }
  
  cx = cy = cz = 0.0;
  for (k=0;k<mris->nvertices;k++) {
    v = &mris->vertices[k];
    cx += v->x;
    cy += v->y;
    cz += v->z;
  }
  cx /= mris->nvertices;
  cy /= mris->nvertices;
  cz /= mris->nvertices;
  
  tcx = tcy = tcz = 0.0;
  for (k=0;k<mris2->nvertices;k++) {
    v2 = &mris2->vertices[k];
    tcx += v2->x;
    tcy += v2->y;
    tcz += v2->z;
  }
  tcx /= mris2->nvertices;
  tcy /= mris2->nvertices;
  tcz /= mris2->nvertices;
  
  really_translate_brain(tcx-cx,tcy-cy,tcz-cz);
  printf("surfer: center of first brain aligned to target brain\n");PR
                      
                      }

void
really_center_brain(void)
{
  VERTEX *v;
  int k;
  float cx,cy,cz;
  
  cx = cy = cz = 0.0;
  for (k=0;k<mris->nvertices;k++) {
    v = &mris->vertices[k];
    cx += v->x;
    cy += v->y;
    cz += v->z;
  }
  cx /= mris->nvertices;
  cy /= mris->nvertices;
  cz /= mris->nvertices;
  
  mris->xctr = mris->zctr = mris->yctr = 0.0;
  really_translate_brain(-cx,-cy,-cz);
  printf("surfer: avg vertex (%f, %f, %f) moved to (0,0,0)\n",cx,cy,cz);PR
                    }

void
really_center_second_brain(void)
{
  VERTEX *v2;
  int k;
  float cx,cy,cz;
  
  if (!secondsurfaceloaded) { 
    printf(
     "surfer: ### really_center_second_brain failed: no second surface read\n");PR
                      return; }
  
  cx = cy = cz = 0.0;
  for (k=0;k<mris2->nvertices;k++) {
    v2 = &mris2->vertices[k];
    cx += v2->x;
    cy += v2->y;
    cz += v2->z;
  }
  cx /= mris2->nvertices;
  cy /= mris2->nvertices;
  cz /= mris2->nvertices;
  
  mris2->xctr = mris2->zctr = mris2->yctr = 0.0;
  
  for (k=0;k<mris2->nvertices;k++) {  /* really translate */
    v2 = &mris2->vertices[k];
    v2->x += -cx;
    v2->y += -cy;
    v2->z += -cz;
  }
  printf("surfer: avg vertex (%f, %f, %f) surf2 moved to (0,0,0)\n",cx,cy,cz);PR
                    }

void
print_real_transform_matrix(void)
{
  int i,j;
  
  printf("surfer: accumulated real transforms\n");
  for (i=0;i<4;i++) {
    for (j=0;j<4;j++) {
      printf(" %13.3e ",reallymat[i][j]);
    }
    printf("\n");
  } PR
      }

void
write_really_matrix(char *dir)
{
  int i,j;
  char fname[NAME_LENGTH];
  FILE *fp;
  
  sprintf(fname,"%s/really.mat",dir);
  fp = fopen(fname,"w");
  if (fp==NULL) {printf("surfer: ### can't create file %s\n",fname);PR return;}
  for (i=0;i<4;i++) {
    for (j=0;j<4;j++) {
      fprintf(fp,"%13.3e ",reallymat[i][j]);
    }
    fprintf(fp,"\n");
  }
  fclose(fp);
  printf("surfer: file %s written\n",fname);PR
                }

void
read_really_matrix(char *dir)
{
  VERTEX *v;
  int i,j,k;
  float a,b,c,d;
  float or[4],r[4];
  char line[NAME_LENGTH];
  char fname[NAME_LENGTH];
  FILE *fp;
  
  sprintf(fname,"%s/really.mat",dir);
  fp = fopen(fname,"r");
  if (fp==NULL) {printf("surfer: ### File %s not found\n",fname);PR return;}
  
  i = 0;
  while (fgets(line,NAME_LENGTH,fp) != NULL) {
    if (sscanf(line,"%f %f %f %f",&a,&b,&c,&d) == 4)  {
      reallymat[i][0] = a;
      reallymat[i][1] = b;
      reallymat[i][2] = c;
      reallymat[i][3] = d;
      i++;
    }
    else {
      printf("surfer: ### couldn't parse this line in matrix file:  %s",line);
      printf("surfer: ###   ...read_really_matrix() failed\n");PR return;
    }
  }
  for (k=0;k<mris->nvertices;k++) {
    v = &mris->vertices[k];
    or[0] = v->x;
    or[1] = v->y;
    or[2] = v->z;
    or[3] = 1.0;
    r[0] = r[1] = r[2] = r[3] = 0.0;
    for (i=0;i<4;i++)
      for (j=0;j<4;j++) {
  r[i] += reallymat[i][j]*or[j];
      }
    v->x = r[0];
    v->y = r[1];
    v->z = r[2];
  }
  print_real_transform_matrix();
  printf("surfer: file %s read,applied\n",fname);PR
               }

void
flatten(char *dir)
{
  float x,y,z,d,d1,d2;
  float nx,ny,nz;
  VERTEX *v;
  int k,an;
  FILE *fp;
  char fname[NAME_LENGTH];
  
  x = y = z = nx = ny = nz = 0;
  an = 0;
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag)
      {
  v = &mris->vertices[k];
  x += v->x;
  y += v->y;
  z += v->z;
  nx += v->nx;
  ny += v->ny;
  nz += v->nz;
  an++;
      }
  x /= an; y /= an; z /= an;
  printf("surfer: avg p = {%f,%f,%f}\n",x,y,z);
  printf("surfer: sum n = {%f,%f,%f}\n",nx,ny,nz);
  /* or override with direct front,back */
  if (project==POSTERIOR) { nx = nz = 0.0; ny = -1.0; }
  if (project==ANTERIOR)  { nx = nz = 0.0; ny = 1.0;  }
  d = sqrt(nx*nx+ny*ny+nz*nz);
  nx /= d; ny /= d; nz /= d;
  d = sqrt(nx*nx+ny*ny);
  printf("surfer: norm n = {%f,%f,%f}\n",nx,ny,nz);
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag)
      {
  v = &mris->vertices[k];
  v->x -= x;
  v->y -= y;
  v->z -= z;
  d1 = sqrt(v->x*v->x+v->y*v->y+v->z*v->z);
  transform(&v->x,&v->y,&v->z,nx,ny,nz,d);
  d2 = sqrt(v->x*v->x+v->y*v->y+v->z*v->z);
  if (fabs(d1-d2)>0.0001) printf("surfer: d1=%f, d2=%f\n",d1,d2);
  transform(&v->nx,&v->ny,&v->nz,nx,ny,nz,d);
      }
  
  /* print transform matrix in tmp dir */
  sprintf(fname,"%s/surfer.mat",dir);
  fp = fopen(fname,"w");
  if (fp==NULL) {
    printf("surfer: ### can't create file %s\n",fname);PR return; }
  else {
    fprintf(fp,"%13.3e %13.3e %13.3e %13.3e\n",  nx*nz/d,  -nx,  ny/d,   0.0);
    fprintf(fp,"%13.3e %13.3e %13.3e %13.3e\n",    d,       nz,   0.0,   0.0);
    fprintf(fp,"%13.3e %13.3e %13.3e %13.3e\n",  -ny*nz/d,  ny,   nx/d,  0.0);
    fprintf(fp,"%13.3e %13.3e %13.3e %13.3e\n",   0.0,      0.0,  0.0,   1.0);
    fclose(fp);
    printf("surfer: file %s written\n",fname);
  }
  
  transform(&nx,&ny,&nz,nx,ny,nz,d);
  printf("surfer: transformed n = {%f,%f,%f}\n",nx,ny,nz);PR
                  for (k=0;k<mris->nvertices;k++)
                    {
                mris->vertices[k].z = 0;
                    }
  compute_normals();
}

void
area_shrink(int niter)  /* consider area */
{
#if 0
  float x,y,z,sx,sy,sz,nc,force,nforce;
  float d,dx,dy,dz,sval,sinval,soutval,snc;
  float nx,ny,nz;
  VERTEX *v,*v1;
  int iter,k,m,n;
  float sd,ad,dmax,dd;
  int navg,an,nclip;
  double area,logarat,avgarea,vararea,minrat,maxrat;
  float ax,ay,az,tx,ty,tz;
  int nneg;
  
  for (iter=0;iter<niter;iter++)
    {
      smooth_logarat(10);
      ad = 0;
      dmax = 0;
      an = 0;
      nclip = 0;
      for (k=0;k<mris->nvertices;k++)
  {
    v = &mris->vertices[k];
    v->tx = v->x;
    v->ty = v->y;
    v->tz = v->z;
    /*      v->onc = v->nc;*/
    v->oripflag = v->ripflag;
  }
      sval = sinval = soutval = snc = 0;
      navg = 0;
      nneg = 0;
      avgarea = vararea = 0;
      maxrat = -100;
      minrat = 100;
      dd = 0;
      for (k=0;k<mris->nvertices;k++)
  if (!mris->vertices[k].oripflag)
    {
      v = &mris->vertices[k];
      x = v->tx;
      y = v->ty;
      z = v->tz;
      nx = v->nx;
      ny = v->ny;
      nz = v->nz;
      area = v->area;
      logarat = v->logarat;
      sx=sy=sz=sd=0;
      ax=ay=az=0;
      tx=ty=tz=0;
      n=0;
      avgarea += logarat;
      vararea += logarat*logarat;
      if (logarat>maxrat) maxrat = logarat;
      if (logarat<minrat) minrat = logarat;
      if (area/v->origarea<0) nneg++;
      
      /*v->curv = logarat;*/   /* marty: out 10/9/97 */
      
      for (m=0;m<v->vnum;m++)
        if (!mris->vertices[v->v[m]].oripflag)
    {
      v1 = &mris->vertices[v->v[m]];
      sx += dx = v1->tx - x;
      sy += dy = v1->ty - y;
      sz += dz = v1->tz - z;
      
      d = sqrt(dx*dx+dy*dy+dz*dz);
      if (d>0)
        {
          ax += (v1->tx - x)/d*(v1->logarat-logarat);
          ay += (v1->ty - y)/d*(v1->logarat-logarat);
          az += (v1->tz - z)/d*(v1->logarat-logarat);
        }
      if (v->border)
        {
          force = -logarat;
          d = sqrt(x*x+y*y+z*z);
          tx += x/d*force;
          ty += y/d*force;
          tz += z/d*force;
        }
      sd += d = sqrt(dx*dx+dy*dy+dz*dz);
      dd += SQR(d-1.0);
      n++;
    }
      if (n>0)
        {
    sx = sx/n;
    sy = sy/n;
    sz = sz/n;
    tx = tx/n;
    ty = ty/n;
    tz = tz/n;
    ax = ax/n;
    ay = ay/n;
    az = az/n;
    sd = sd/n;
        }
      if ((d=sqrt(ax*ax+ay*ay+az*az))>1.0)
        {
    ax /= d;
    ay /= d;
    az /= d;
        }
      force = 0;
      nc = sx*nx+sy*ny+sz*nz;
      sx -= nc*nx;
      sy -= nc*ny;
      sz -= nc*nz;
      snc += nc;
      v->nc = nc;
      nforce = 0;
      
      v->val = nc;
      
      if (logarat<0)
        {
    nforce = -logarat;
        }
      if (ncthreshflag)
        {
    if (nc<0)
      nc = (nc<-ncthresh)?nc+ncthresh:0; 
    else 
      nc = (nc>ncthresh)?nc-ncthresh:0; 
        }
      dx = tx*wt + ax*wa + sx*ws + nx*nc*wn + nx*nforce*wc;
      dy = ty*wt + ay*wa + sy*ws + ny*nc*wn + ny*nforce*wc;
      dz = tz*wt + az*wa + sz*ws + nz*nc*wn + nz*nforce*wc;
      if (momentumflag)
        {
    dx = decay*v->odx+update*dx;
    dy = decay*v->ody+update*dy;
    dz = decay*v->odz+update*dz;
        }
      d=sqrt(dx*dx+dy*dy+dz*dz);
      /*
        if (!(d<1))
        printf("surfer: k=%d: d=%f, (%f,%f,%f), s=(%f,%f,%f), a=(%f,%f,%f)\n",
        k,d,dx,dy,dz,sd,sy,sz,ax,ay,az);
      */
      if (d>1.0)
        {
    nclip ++;
    dx /= d;
    dy /= d;
    dz /= d;
        }
      v->odx = dx;
      v->ody = dy;
      v->odz = dz;
      d=sqrt(dx*dx+dy*dy+dz*dz);
      if (d>dmax) dmax = d;
      ad += d;
      an ++;
      if (!(v->border&&fixed_border))
        {
    v->x += dx;
    v->y += dy;
    v->z += dz;
        }
    }
      ad /= an;
      snc /= an;
      avgarea /= an;
      vararea = sqrt(vararea/an - avgarea*avgarea);
      dd = sqrt(dd/an);
      
      compute_normals();
      
      printf("surfer: %d: ad=%f, dmax=%f, snc=%f dd=%f\n",iter,ad,dmax,snc,dd);
      printf("surfer:    avg=%f, var=%f, min=%f, max=%f, nneg=%d\n",
       avgarea,vararea,minrat,maxrat,nneg);PR
               }
#endif
}

void
shrink2d(int niter)
{
#if 0
  float x,y,z,sx,sy,sz,nc,force,nforce;
  float d,dx,dy,dz,sval,sinval,soutval,snc;
  float nx,ny,nz;
  VERTEX *v,*v1;
  int iter,k,m,n;
  float sd,ad,dmax,dd;
  int navg,an,nclip;
  double area,logarat,avgarea,vararea,minrat,maxrat;
  double shearx,sheary,logshear,avgshear,varshear,minshear,maxshear;
  float ax,ay,az,shx,shy,shz,tx,ty,tz,tnx,tny,tnz,f1,f2;
  int nneg;
  
  if (!computed_shear_flag)
    {
      smooth_logarat(10);
      compute_shear();
      smooth_shear(10);
      compute_boundary_normals();
      smooth_boundary_normals(10);
      computed_shear_flag = TRUE;
    }
  
  for (iter=0;iter<niter;iter++)
    {
      /*
  smooth_momentum(10);
      */
      ad = 0;
      dmax = 0;
      an = 0;
      nclip = 0;
      for (k=0;k<mris->nvertices;k++)
  {
    v = &mris->vertices[k];
    v->tx = v->x;
    v->ty = v->y;
    v->tz = v->z;
    /*      v->onc = v->nc;*/
    v->oripflag = v->ripflag;
  }
      sval = sinval = soutval = snc = 0;
      navg = 0;
      nneg = 0;
      avgarea = vararea = 0;
      maxrat = -100;
      minrat = 100;
      avgshear = varshear = 0;
      maxshear = -100;
      minshear = 100;
      dd = 0;
      for (k=0;k<mris->nvertices;k++)
  if (!mris->vertices[k].ripflag)
    {
      v = &mris->vertices[k];
      x = v->tx;
      y = v->ty;
      z = v->tz;
      nx = v->nx;
      ny = v->ny;
      nz = v->nz;
      area = v->area;
      logarat = v->logarat;
      logshear = v->logshear;
      shearx = v->shearx;
      sheary = v->sheary;
      sx=sy=sz=sd=0;
      ax=ay=az=0;
      shx=shy=shz=0;
      tnx=tny=tnz=0;
      tx=ty=tz=0;
      n=0;
      avgarea += logarat;
      vararea += logarat*logarat;
      if (logarat>maxrat) maxrat = logarat;
      if (logarat<minrat) minrat = logarat;
      
      avgshear += logshear;
      varshear += logshear*logshear;
      if (logshear>maxshear) maxshear = logshear;
      if (logshear<minshear) minshear = logshear;
      /*
        v->curv = logarat;
        v->curv = logshear;
      */
      
      for (m=0;m<v->vnum;m++)
        if (!mris->vertices[v->v[m]].oripflag)
    {
      v1 = &mris->vertices[v->v[m]];
      sx += dx = v1->tx - x;
      sy += dy = v1->ty - y;
      sz += dz = v1->tz - z;
      
      d = sqrt(dx*dx+dy*dy+dz*dz);
      if (!v->border)
        {
          if (d>0)
      {
        ax += (v1->tx - x)/d*(v1->logarat-logarat);
        ay += (v1->ty - y)/d*(v1->logarat-logarat);
        az += (v1->tz - z)/d*(v1->logarat-logarat);
      }
          f1 = fabs((v1->tx-x)*v1->shearx+(v1->ty-y)*v1->sheary)-
      fabs(-(v1->tx-x)*v1->sheary+(v1->ty-y)*v1->shearx);
          f2 = fabs((v1->tx-x)*shearx+(v1->ty-y)*sheary)-
      fabs(-(v1->tx-x)*sheary+(v1->ty-y)*shearx);
          if (d>0)
      {
        shx += (v1->tx-x)/d*(f1-f2);
        shy += (v1->ty-y)/d*(f1-f2);
        shz += 0;
      }
        }
      if (v->border)
        {
          force = -logarat;
          if (v->nz<0) {force = 0.0; nneg++;}
          tx += v->bnx*force;
          ty += v->bny*force;
          tz += 0;
          f1 = -(fabs((v->bnx)*shearx+(v->bny)*sheary)-
           fabs(-(v->bnx)*sheary+(v->bny)*shearx));
          if (v->nz<0) {f1 = 0.0;}
          tnx += v->bnx*f1;
          tny += v->bny*f1;
          tnz += 0;
        }
      sd += d = sqrt(dx*dx+dy*dy+dz*dz);
      dd += SQR(d-1.0);
      n++;
    }
      if (n>0)
        {
    sx = sx/n;
    sy = sy/n;
    sz = sz/n;
    tx = tx/n;
    ty = ty/n;
    tz = tz/n;
    tnx = tnx/n;
    tny = tny/n;
    tnz = tnz/n;
    ax = ax/n;
    ay = ay/n;
    az = az/n;
    shx = shx/n;
    shy = shy/n;
    shz = shz/n;
    sd = sd/n;
        }
      if ((d=sqrt(ax*ax+ay*ay+az*az))>1.0)
        {
    ax /= d;
    ay /= d;
    az /= d;
        }
      if ((d=sqrt(shx*shx+shy*shy+shz*shz))>1.0)
        {
    shx /= d;
    shy /= d;
    shz /= d;
        }
      force = 0;
      nc = sx*nx+sy*ny+sz*nz;
      sx -= nc*nx;
      sy -= nc*ny;
      sz -= nc*nz;
      snc += nc;
      v->nc = nc;
      nforce = 0;
      /*
        v->val = nc;
      */
      /* mgh: omit !, then comment out */
      /*
        if (!v->border)
        {
        bnc = sx*v->bnx+sy*v->bny;
        sx -= bnc*nx;
        sy -= bnc*ny;
        }
      */
      
      if (logarat<0)
        {
    nforce = -logarat;
        }
      if (nc<0)
        nc = (nc<-ncthresh)?nc+ncthresh:0; 
      else 
        nc = (nc>ncthresh)?nc-ncthresh:0; 
      dx = tx*wt + ax*wa + sx*ws + shx*wsh + tnx*wbn;
      dy = ty*wt + ay*wa + sy*ws + shy*wsh + tny*wbn;
      dz = tz*wt + az*wa + sz*ws + shz*wsh + tnz*wbn;
      if (momentumflag)
        {
    dx = decay*v->odx+update*dx;
    dy = decay*v->ody+update*dy;
    dz = decay*v->odz+update*dz;
        }
      d=sqrt(dx*dx+dy*dy+dz*dz);
      if (d>1.0)
        {
    nclip ++;
    dx /= d;
    dy /= d;
    dz /= d;
        }
      v->odx = dx;
      v->ody = dy;
      v->odz = dz;
      /*
        v->odx = 0.8*dx+0.5*v->smx;
        v->ody = 0.8*dy+0.5*v->smy;
        v->odz = 0.8*dz+0.5*v->smz;
      */
      d=sqrt(v->odx*v->odx+v->ody*v->ody+v->odz*v->odz);
      if (d>1.0)
        {
    v->odx /= d;
    v->ody /= d;
    v->odz /= d;
        }
      d=sqrt(dx*dx+dy*dy+dz*dz);
      if (d>dmax) dmax = d;
      
      if (!((d>=0)||(d<=0)))
        printf("surfer: d=%f, a:(%f,%f,%f), sh:(%f,%f,%f), s:(%f,%f,%f)\n",  
         d,ax,ay,az,shx,shy,shz,sx,sy,sz);
      
      ad += d;
      an ++;
      if (!(v->border&&fixed_border))
        {
    v->x += dx;
    v->y += dy;
    v->z += dz;
        }
    }
      ad /= an;
      snc /= an;
      avgarea /= an;
      vararea = sqrt(vararea/an - avgarea*avgarea);
      avgshear /= an;
      varshear = sqrt(varshear/an - avgshear*avgshear);
      dd = sqrt(dd/an);
      
      compute_normals();
      smooth_logarat(10);
      compute_shear();
      smooth_shear(10);
      compute_boundary_normals();
      smooth_boundary_normals(10);
      
      printf("surfer: %d: ad=%f, dmax=%f, dd=%f nneg=%d\n",iter,ad,dmax,dd,nneg);
      printf("surfer:     area:  avg=%f, var=%f, min=%f, max=%f\n",
       avgarea,vararea,minrat,maxrat);
      printf("surfer:     shear: avg=%f, var=%f, min=%f, max=%f\n",
       avgshear,varshear,minshear,maxshear);PR
                }
#endif
}

void
sphere_shrink(int niter, float rad)
{
  float x,y,z,sx,sy,sz;
  float d,dx,dy,dz;
  float xc,yc,zc,r,dr;
  VERTEX *v;
  int iter,k,m,n;
  float sd,ad,dmax;
  int an,nclip;
  
  for (iter=0;iter<niter;iter++)
    {
      ad = 0;
      dmax = 0;
      an = 0;
      nclip = 0;
      for (k=0;k<mris->nvertices;k++)
  {
    v = &mris->vertices[k];
    v->tx = v->x;
    v->ty = v->y;
    v->tz = v->z;
  }
      for (k=0;k<mris->nvertices;k++)
  {
    v = &mris->vertices[k];
    x = v->tx;
    y = v->ty;
    z = v->tz;
    sx=sy=sz=sd=0;
    n=0;
    for (m=0;m<v->vnum;m++)
      {
        sx += dx = mris->vertices[v->v[m]].tx - x;
        sy += dy = mris->vertices[v->v[m]].ty - y;
        sz += dz = mris->vertices[v->v[m]].tz - z;
        sd += sqrt(dx*dx+dy*dy+dz*dz);
        n++;
      }
    if (n>0)
      {
        sx = sx/n;
        sy = sy/n;
        sz = sz/n;
        sd = sd/n;
      }
    xc = x+mris->xctr;
    yc = y-mris->yctr;
    zc = z-mris->zctr;
    r = sqrt(xc*xc+yc*yc+zc*zc);
    if (r==0) r=0.00001;
    dr = (rad-r)/rad;
    dx = sx*0.5 + xc/r*dr;
    dy = sy*0.5 + yc/r*dr;
    dz = sz*0.5 + zc/r*dr;
    if (momentumflag)
      {
        dx = decay*v->odx+update*dx;
        dy = decay*v->ody+update*dy;
        dz = decay*v->odz+update*dz;
        if ((d=sqrt(dx*dx+dy*dy+dz*dz))>1.0)
    {
      nclip ++;
      dx /= d;
      dy /= d;
      dz /= d;
    }
        v->odx = dx;
        v->ody = dy;
        v->odz = dz;
      }
    d=sqrt(dx*dx+dy*dy+dz*dz);
    if (d>dmax) dmax = d;
    ad += d;
    an ++;
    v->x += dx;
    v->y += dy;
    v->z += dz;
  }
      printf("surfer: %d: ad=%f, dmax=%f, nclip=%d\n",iter,ad/an,dmax,nclip);PR
                         }
  compute_normals();
}

/* a=rh/lh, b=ant/post,  c=sup/inf */
void
ellipsoid_project(float a, float b, float c)
{
  VERTEX *v;
  int k;
  float x,y,z,x2,y2,z2,dx,dy,dz,a2,b2,c2,a4,b4,c4,a6,b6,c6;
  float f,g,h,d,dist,avgdist=0;
  
  printf("ellipsoid_project(%f,%f,%f)\n",a,b,c);
  
  a2 = a*a;
  b2 = b*b;
  c2 = c*c;
  a4 = a2*a2;
  b4 = b2*b2;
  c4 = c2*c2;
  a6 = a2*a4;
  b6 = b2*b4;
  c6 = c2*c4;
  
  for (k=0;k<mris->nvertices;k++) {
    v = &mris->vertices[k];
    /*
      printf("%6d: before: %6.2f\n",k,SQR(v->x/a)+SQR(v->y/b)+SQR(v->z/c));
    */
    x = v->x;
    y = v->y;
    z = v->z;
    x2 = x*x;
    y2 = y*y;
    z2 = z*z;
    f = x2/a6+y2/b6+z2/c6;
    g = 2*(x2/a4+y2/b4+z2/c4);
    h = x2/a2+y2/b2+z2/c2-1;
    d = (-g+sqrt(g*g-4*f*h))/(2*f);
    dx = d*x/a2;
    dy = d*y/b2;
    dz = d*z/c2;
    v->x = x+dx;
    v->y = y+dy;
    v->z = z+dz;
    dist = sqrt(dx*dx+dy*dy+dz*dz);
    avgdist += dist;
    /*
      printf("%6d: after: %6.2f\n",k,SQR(v->x/a)+SQR(v->y/b)+SQR(v->z/c));
    */
  }
  /*
    printf("ellipsoid_project: avgdist = %f\n",avgdist/mris->nvertices);
  */
  compute_normals();
}

/* a=rh/lh, b=ant/post,  c=sup/inf */
void
ellipsoid_morph(int niter, float a, float b, float c)
{
  VERTEX *v;
  int imnr,i,j,iter,k,m,n,dk,di,dj;
  int an,nclip,delcurvdefined;
  float x,y,z,sx,sy,sz,val;
  float xnei,ynei,znei,gradx,grady,gradz;
  float d,dx,dy,dz;
  float sd,ad,dmax;
  float curv,icurv = 0.0f;
  float icrange,crange;
  double gradnormsum,rmscurv,rmsicurv,rmscurverr,curvfact,rmsinum;
  
  if (curvloaded && curvimloaded) {
    icrange = mris2->max_curv-mris2->min_curv;
    crange = mris->max_curv-mris->min_curv;
  }
  
  printf("ellipsoid_morph(%d,%f,%f,%f)\n",niter,a,b,c);
  
  ellipsoid_project(a,b,c);
  rmscurv = rmsicurv = rmsinum = 0;
  if (curvimflag && curvimloaded)
    for (imnr=0;imnr<numimg;imnr++)
      for (i=0;i<IMGSIZE;i++)
  for (j=0;j<IMGSIZE;j++)
    if (curvimflags[imnr][i][j] & CURVIM_DEFINED)
      {
        icurv = bytetofloat(curvim[imnr][i][j],mris2->min_curv,mris2->max_curv);
        rmsicurv += SQR(icurv);
        rmsinum++;
      }
  rmsicurv = sqrt(rmsicurv/rmsinum);
  for (k=0;k<mris->nvertices;k++) {
    v = &mris->vertices[k];
    curv = v->curv;
    rmscurv += SQR(curv);
  }
  rmscurv = sqrt(rmscurv/mris->nvertices);
  curvfact = rmsicurv/rmscurv;
  printf("rmscurv = %f, rmsicurv = %f, curvfact = %f\n",
   rmscurv,rmsicurv,curvfact);
  for (k=0;k<mris->nvertices;k++) {
    v = &mris->vertices[k];
    v->curv *= curvfact;
  }
  for (iter=0;iter<niter;iter++) {
    ad = 0;
    dmax = 0;
    an = 0;
    nclip = 0;
    for (k=0;k<mris->nvertices;k++) {
      v = &mris->vertices[k];
      v->tx = v->x;
      v->ty = v->y;
      v->tz = v->z;
    }
    gradnormsum = rmsicurv = rmscurverr = 0;
    for (k=0;k<mris->nvertices;k++) {
      v = &mris->vertices[k];
      x = v->tx;
      y = v->ty;
      z = v->tz;
      sx=sy=sz=sd=0;
      n=0;
      if (curvimflag && curvimloaded) {
        delcurvdefined = TRUE;
        imnr = (int)((y-yy0)/st+0.5);
        i = (int)((zz1-z)/ps+0.5);
        j = (int)((xx1-x)/ps+0.5);
        imnr = (imnr<0)?0:(imnr>=numimg)?numimg-1:imnr;
        i = (i<0)?0:(i>=IMGSIZE)?IMGSIZE-1:i;
        j = (j<0)?0:(j>=IMGSIZE)?IMGSIZE-1:j;
        if (curvimflags[imnr][i][j] & CURVIM_DEFINED)
          icurv = bytetofloat(curvim[imnr][i][j],mris2->min_curv,mris2->max_curv);
        else
          delcurvdefined = FALSE;
        curv = v->curv;
        rmsicurv += SQR(icurv);
        rmscurverr += SQR(curv-icurv);
        gradx = grady = gradz = 0;
        for (dk=-1;dk<=1;dk++)
    for (di=-1;di<=1;di++)
      for (dj=-1;dj<=1;dj++)
        {
    if (!(curvimflags[imnr+dk][i+di][j+dj] & CURVIM_DEFINED))
      delcurvdefined = FALSE;
    val = bytetofloat(curvim[imnr+dk][i+di][j+dj],mris2->min_curv,mris2->max_curv);
    gradx += -dj*val;
    grady +=  dk*val;
    gradz += -di*val;
        }
        if (!delcurvdefined)
          printf("undefined gradient at vertex %d (%f,%f,%f)\n",k,x,y,z);
        gradx /= 3*3*2;
        grady /= 3*3*2;
        gradz /= 3*3*2;
        sx += icstrength*gradx*(curv-icurv);
        sy += icstrength*grady*(curv-icurv);
        sz += icstrength*gradz*(curv-icurv);
        gradnormsum += sqrt(gradx*gradx+grady*grady+gradz*gradz);
      }
      for (m=0;m<v->vnum;m++) {
        xnei = mris->vertices[v->v[m]].tx;
        ynei = mris->vertices[v->v[m]].ty;
        znei = mris->vertices[v->v[m]].tz;
        sx += dx = xnei - x;
        sy += dy = ynei - y;
        sz += dz = znei - z;
        sd += sqrt(dx*dx+dy*dy+dz*dz);
        n++;
      }
      if (n>0) {
        sx = sx/n;
        sy = sy/n;
        sz = sz/n;
        sd = sd/n;
      }
      dx = sx;
      dy = sy;
      dz = sz;
      if (momentumflag) {
        dx = decay*v->odx+update*dx;
        dy = decay*v->ody+update*dy;
        dz = decay*v->odz+update*dz;
        if ((d=sqrt(dx*dx+dy*dy+dz*dz))>1.0) {
          nclip ++;
          dx /= d;
          dy /= d;
          dz /= d;
        }
        v->odx = dx;
        v->ody = dy;
        v->odz = dz;
      }
      d=sqrt(dx*dx+dy*dy+dz*dz);
      if (d>dmax) dmax = d;
      ad += d;
      an ++;
      v->x += dx;
      v->y += dy;
      v->z += dz;
    }
    printf("surfer: %d: ad=%f, dmax=%f, nclip=%d\n",iter,ad/an,dmax,nclip);PR
                       ellipsoid_project(a,b,c);
    printf("average gradient = %f, curvature error = %e\n",
     gradnormsum/mris->nvertices,sqrt(rmscurverr/rmsicurv));
  }
  compute_normals();
}

/* a=rh/lh, b=ant/post,  c=sup/inf */
void
ellipsoid_shrink(int niter, float a, float b, float c)
{
  VERTEX *v;
  int imnr,i,j,iter,k,m,n;
  int an,nclip,delcurvdefined = 0;
  float x,y,z,sx,sy,sz;
  float xnei,ynei,znei;
  float d,dx,dy,dz;
  float xc,yc,zc,r,dr;
  float sd,ad,dmax;
  float acx,acy,acz;
  float sqd,rad;
  float curv,icurv,icurvnei,cfact;
  float icrange,crange;
  
  curv = icurv = icurvnei = 0.0f ;
  if (curvloaded && curvimloaded) {
    icrange = mris2->max_curv-mris2->min_curv;
    crange = mris->max_curv-mris->min_curv;
  }
  
  acx = acy = acz = 0.0;
  for (k=0;k<mris->nvertices;k++) {
    v = &mris->vertices[k];
    acx += v->x;
    acy += v->y;
    acz += v->z;
  }
  acx /= mris->nvertices;
  acy /= mris->nvertices;
  acz /= mris->nvertices;
  
  for (iter=0;iter<niter;iter++) {
    ad = 0;
    dmax = 0;
    an = 0;
    nclip = 0;
    for (k=0;k<mris->nvertices;k++) {
      v = &mris->vertices[k];
      v->tx = v->x;
      v->ty = v->y;
      v->tz = v->z;
    }
    for (k=0;k<mris->nvertices;k++) {
      v = &mris->vertices[k];
      x = v->tx;
      y = v->ty;
      z = v->tz;
      sx=sy=sz=sd=0;
      n=0;
      if (curvimflag && curvimloaded) {
        delcurvdefined = TRUE;
        imnr = (int)((y-yy0)/st+0.5);
        i = (int)((zz1-z)/ps+0.5);
        j = (int)((xx1-x)/ps+0.5);
        imnr = (imnr<0)?0:(imnr>=numimg)?numimg-1:imnr;
        i = (i<0)?0:(i>=IMGSIZE)?IMGSIZE-1:i;
        j = (j<0)?0:(j>=IMGSIZE)?IMGSIZE-1:j;
        if (curvimflags[imnr][i][j] & CURVIM_DEFINED)
          icurv = bytetofloat(curvim[imnr][i][j],mris2->min_curv,mris2->max_curv);
        else
          delcurvdefined = FALSE;
        curv = v->curv;
      }
      for (m=0;m<v->vnum;m++) {
        xnei = mris->vertices[v->v[m]].tx;
        ynei = mris->vertices[v->v[m]].ty;
        znei = mris->vertices[v->v[m]].tz;
        if (curvimflag && curvimloaded) {
          imnr = (int)((ynei-yy0)/st+0.5);
          i = (int)((zz1-znei)/ps+0.5);
          j = (int)((xx1-xnei)/ps+0.5);
          imnr = (imnr<0)?0:(imnr>=numimg)?numimg-1:imnr;
          i = (i<0)?0:(i>=IMGSIZE)?IMGSIZE-1:i;
          j = (j<0)?0:(j>=IMGSIZE)?IMGSIZE-1:j;
          if (curvimflags[imnr][i][j] & CURVIM_DEFINED)
            icurvnei = bytetofloat(curvim[imnr][i][j],mris2->min_curv,mris2->max_curv);
          else
            delcurvdefined = FALSE;
          /*curvnei = mris->vertices[v->v[m]].curv;*/   /* two dels?? */
          cfact = 1.0;
          if (delcurvdefined)
            cfact += icstrength*(icurvnei-icurv)*(curv-icurv);
    /* del im w/only sign of im/loc diff */
    /*cfact += (icurvnei-icurv) * copysign(icstrength,curv-icurv);*/
    /*cfact += (icurvnei-icurv)/icrange
     * copysign(icstrength,mris2->min_curv+curv*(icrange/crange) - icurv);*/
          sx += dx = (xnei - x)*cfact;
          sy += dy = (ynei - y)*cfact;
          sz += dz = (znei - z)*cfact;
        }
        else {
          sx += dx = xnei - x;
          sy += dy = ynei - y;
          sz += dz = znei - z;
        }
        sd += sqrt(dx*dx+dy*dy+dz*dz);
        n++;
      }
      if (n>0) {
        sx = sx/n;
        sy = sy/n;
        sz = sz/n;
        sd = sd/n;
      }
      xc = x-acx;
      yc = y-acy;
      zc = z-acz;
      r = sqrt(xc*xc+yc*yc+zc*zc);
      if (r==0) r=0.00001;
      sqd = SQR(xc/a) + SQR(yc/b) + SQR(zc/c);
      rad = sqrt(SQR(xc)/sqd + SQR(yc)/sqd + SQR(zc)/sqd);  /* ellipsoid */
      dr = (rad-r)/rad;
      dx = sx*0.5 + xc/r*dr;      /* radial (tangential comp on ellipsoid) */
      dy = sy*0.5 + yc/r*dr;
      dz = sz*0.5 + zc/r*dr;
#if 0
      if (dr > 0.01 || dr < -0.01) {
        dx = sx*0.5 + xc/r*dr;   /* radial approach */
        dy = sy*0.5 + yc/r*dr;
        dz = sz*0.5 + zc/r*dr;
      }
      else {
        dx = sx*0.5 + dr*v->nx;  /* normal there (unstable if folded) */
        dy = sy*0.5 + dr*v->ny;
        dz = sz*0.5 + dr*v->nz;
      }
#endif
      if (momentumflag) {
        dx = decay*v->odx+update*dx;
        dy = decay*v->ody+update*dy;
        dz = decay*v->odz+update*dz;
        if ((d=sqrt(dx*dx+dy*dy+dz*dz))>1.0) {
          nclip ++;
          dx /= d;
          dy /= d;
          dz /= d;
        }
        v->odx = dx;
        v->ody = dy;
        v->odz = dz;
      }
      d=sqrt(dx*dx+dy*dy+dz*dz);
      if (d>dmax) dmax = d;
      ad += d;
      an ++;
      v->x += dx;
      v->y += dy;
      v->z += dz;
    }
    printf("surfer: %d: ad=%f, dmax=%f, nclip=%d\n",iter,ad/an,dmax,nclip);PR
                       }
  compute_normals();
}

/* 50 ellipsoid_shrink(2,100,150); */
void
ellipsoid_shrink_bug(int niter, float rad, float len)
{
  float x,y,z,sx,sy,sz;
  float d,dx,dy,dz;
  float xc,yc,zc,r,dr;
  float ex,ey,ez;
  float acx,acy,acz;
  VERTEX *v;
  int iter,k,m,n;
  float sd,ad,dmax;
  int an,nclip;
  
  ex = 0.0;
  ey = (len-rad)/2.0;
  ez = 0.0;
  
  acx = acy = acz = 0.0;
  for (k=0;k<mris->nvertices;k++)
    {
      v = &mris->vertices[k];
      acx += v->x;
      acy += v->y;
      acz += v->z;
    }
  acx /= (float)mris->nvertices;
  acy /= (float)mris->nvertices;
  acz /= (float)mris->nvertices;
  
  for (iter=0;iter<niter;iter++)
    {
      ad = 0;
      dmax = 0;
      an = 0;
      nclip = 0;
      for (k=0;k<mris->nvertices;k++)
  {
    v = &mris->vertices[k];
    v->tx = v->x;
    v->ty = v->y;
    v->tz = v->z;
  }
      for (k=0;k<mris->nvertices;k++)
  {
    v = &mris->vertices[k];
    x = v->tx;
    y = v->ty;
    z = v->tz;
    sx=sy=sz=sd=0;
    n=0;
    for (m=0;m<v->vnum;m++)
      {
        sx += dx = mris->vertices[v->v[m]].tx - x;
        sy += dy = mris->vertices[v->v[m]].ty - y;
        sz += dz = mris->vertices[v->v[m]].tz - z;
        sd += sqrt(dx*dx+dy*dy+dz*dz);
        n++;
      }
    if (n>0)
      {
        sx = sx/n;
        sy = sy/n;
        sz = sz/n;
        sd = sd/n;
      }
    /*
      xc = x+mris->xctr;
      yc = y-mris->yctr;
      zc = z-mris->zctr;
    */
    xc = x-acx + (x-acx)*ex;  /* sphere + sphere dot ellipse */
    yc = y-acy + (y-acy)*ey;
    zc = z-acz + (z-acz)*ez;
    r = sqrt(xc*xc+yc*yc+zc*zc);
    if (r==0) r=0.00001;
    dr = (rad-r)/rad;
    dx = sx*0.5 + xc/r*dr;
    dy = sy*0.5 + yc/r*dr;
    dz = sz*0.5 + zc/r*dr;
    if (momentumflag)
      {
        dx = decay*v->odx+update*dx;
        dy = decay*v->ody+update*dy;
        dz = decay*v->odz+update*dz;
        if ((d=sqrt(dx*dx+dy*dy+dz*dz))>1.0)
    {
      nclip ++;
      dx /= d;
      dy /= d;
      dz /= d;
    }
        v->odx = dx;
        v->ody = dy;
        v->odz = dz;
      }
    d=sqrt(dx*dx+dy*dy+dz*dz);
    if (d>dmax) dmax = d;
    ad += d;
    an ++;
    v->x += dx;
    v->y += dy;
    v->z += dz;
  }
      printf("surfer: %d: ad=%f, dmax=%f, nclip=%d\n",iter,ad/an,dmax,nclip);PR
                         }
  compute_normals();
}

void
compute_curvature(void)
{
  float x,y,z,dx,dy,dz,r;
  VERTEX *v;
  int k,m;
  
  for (k=0;k<mris->nvertices;k++)
    {
      v = &mris->vertices[k];
      x = v->x;
      y = v->y;
      z = v->z;
      v->curv = 0;
      for (m=0;m<v->vnum;m++)
  {
    dx = mris->vertices[v->v[m]].x-x;
    dy = mris->vertices[v->v[m]].y-y;
    dz = mris->vertices[v->v[m]].z-z;
    r = sqrt(dx*dx+dy*dy+dz*dz);
    if (r>0)
      v->curv += (dx*v->nx+dy*v->ny+dz*v->nz)/r;
  }
    }
  
  /* begin rkt */
  enable_menu_set (MENUSET_CURVATURE_LOADED, 1);
  /* end rkt */
}

void
clear_curvature(void)
{
  VERTEX *v;
  int k;
  
  for (k=0;k<mris->nvertices;k++)
    {
      v = &mris->vertices[k];
      v->curv = 0;
    }
}

void
normalize_area(void)
{
  VERTEX *v;
  int k;
  float a,oa,f;
  
  if (MRIflag && MRIloaded) {
    printf(
     "surfer: ### normalize_area not done w/MRIflag: misaligns surface\n");PR
                       return; }
  
  oa = a = 0;
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag)
      {
  v = &mris->vertices[k];
  a += v->area;
  oa += v->origarea;
      }
  f = sqrt(oa/a);
  printf("surfer: oa=%f sqmm, a=%f sqmm, f=%f\n",oa/2,a/2,f);PR
                     for (k=0;k<mris->nvertices;k++)
                 if (!mris->vertices[k].ripflag)
                   {
                     v = &mris->vertices[k];
                     v->x *= f;
                     v->y *= f;
                     v->z *= f;
                   }
  compute_normals();
}

void
normalize_surface(void)
{
  VERTEX *v;
  int i,j,k;
  float x,y,z;
  float minx,maxx,miny,maxy,minz,maxz;
  float minx2,maxx2,miny2,maxy2,minz2,maxz2;
  
  minx2 = miny2 = minz2 = 1000;
  maxx2 = maxy2 = maxz2 = -1000;
  for (k=1;k<numimg-1;k++)
    for (i=1;i<IMGSIZE-1;i++)
      for (j=1;j<IMGSIZE-1;j++)
  {
    x = IMGSIZE/2.0-j;
    z = IMGSIZE/2.0-i;
    y = k-numimg/2.0;
    if (im[k][i][j]!=0)
      {
        if (x>maxx2) maxx2 = x;
        if (x<minx2) minx2 = x;
        if (y>maxy2) maxy2 = y;
        if (y<miny2) miny2 = y;
        if (z>maxz2) maxz2 = z;
        if (z<minz2) minz2 = z;
      }
  }
  minx = miny = minz = 1000;
  maxx = maxy = maxz = -1000;
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag)
      {
  v = &mris->vertices[k];
  if (v->x>maxx) maxx = v->x;
  if (v->x<minx) minx = v->x;
  if (v->y>maxy) maxy = v->y;
  if (v->y<miny) miny = v->y;
  if (v->z>maxz) maxz = v->z;
  if (v->z<minz) minz = v->z;
      }
  printf("surfer: minx2=%f, maxx2=%f\n",minx2,maxx2);
  printf("surfer: miny2=%f, maxy2=%f\n",miny2,maxy2);
  printf("surfer: minz2=%f, maxz2=%f\n",minz2,maxz2);
  printf("surfer: minx=%f, maxx=%f\n",minx,maxx);
  printf("surfer: miny=%f, maxy=%f\n",miny,maxy);
  printf("surfer: minz=%f, maxz=%f\n",minz,maxz);
  PR
    for (k=0;k<mris->nvertices;k++)
      if (!mris->vertices[k].ripflag)
  {
    v = &mris->vertices[k];
    v->x = minx2+(v->x-minx)*(maxx2-minx2)/(maxx-minx);
    v->y = miny2+(v->y-miny)*(maxy2-miny2)/(maxy-miny);
    v->z = minz2+(v->z-minz)*(maxz2-minz2)/(maxz-minz);
  }
  compute_normals();
}

void
load_brain_coords(float x, float y, float z, float v[])
{
  v[0] = -x;
  v[1] = z;
  v[2] = y;
}

int outside(float x,float y, float z)
{
  return (x<xmin||x>xmax
    ||y<ymin||y>ymax
    ||-z<zmin||-z>zmax);
}

#ifdef USE_VERTEX_ARRAYS
static GLfloat *vertices = NULL ;
static GLfloat *normals = NULL ;

static float *colors=0;
static float *mesh_colors=0;

static void fill_color_array(MRI_SURFACE *mris, float *colors);
static GLuint *faces ;
static int init_vertex_arrays(MRI_SURFACE *mris) ;

static int get_color_vals(float val, float curv, int mode, GLubyte *r, GLubyte *g, GLubyte *b) ;
static void get_complexval_color_vals(float x, float y, float stat, float curv,GLubyte *r, GLubyte *g, GLubyte *b) ;
static int fill_vertex_arrays(MRI_SURFACE *mris) ;
static int
init_vertex_arrays(MRI_SURFACE *mris)
{
#ifndef IRIX
  colors = (float *)calloc(3*VERTICES_PER_FACE*mris->nfaces, sizeof(float));
  if (!colors)
    ErrorExit(ERROR_NOMEMORY, "init_vertex_arrays: calloc failed") ;
  
  vertices = (GLfloat *)calloc(3*mris->nvertices, sizeof(GLfloat)) ;
  normals = (GLfloat *)calloc(3*mris->nvertices, sizeof(GLfloat)) ;
  faces = (GLuint *)calloc(VERTICES_PER_FACE*mris->nfaces, sizeof(unsigned int)) ;
  if (!vertices || !faces || !colors || !normals)
    ErrorExit(ERROR_NOMEMORY, "init_vertex_arrays: calloc failed") ;
  
  fill_vertex_arrays(mris) ;
  
  /* glEnableClientState ( GL_NORMAL_ARRAY ); */
  
  glEnableClientState ( GL_VERTEX_ARRAY );
  glEnableClientState ( GL_COLOR_ARRAY );
  glEnableClientState ( GL_NORMAL_ARRAY );
  glVertexPointer(3, GL_FLOAT, 0, vertices) ;
  glNormalPointer(GL_FLOAT, 0, normals) ;
  glColorPointer(3, GL_FLOAT, 0, colors);
#endif
  return(NO_ERROR) ;
}

static int init_mesh_colors(MRI_SURFACE *mris)
{
  int i;
  mesh_colors = (float *)calloc(3*VERTICES_PER_FACE*mris->nfaces, sizeof(float));
  if(!mesh_colors)
    ErrorExit(ERROR_NOMEMORY, "init_mesh_colors: calloc failed") ;
  
  for(i=0; i<mris->nvertices; i++) {
    mesh_colors[3*i] = (float)meshr/255.0;
    mesh_colors[3*i+1] = (float)meshg/255.0;
    mesh_colors[3*i+2] = (float)meshb/255.0;
  }
  return(NO_ERROR) ;
}

static int fill_mesh_colors()
{
  int i;
  for(i=0; i<mris->nvertices; i++) {
    mesh_colors[3*i] = (float)meshr/255.0;
    mesh_colors[3*i+1] = (float)meshg/255.0;
    mesh_colors[3*i+2] = (float)meshb/255.0;
  }
  return(NO_ERROR) ;
}
static int 
fill_vertex_arrays(MRI_SURFACE *mris)
     
{
  int    vno, fno, i, n ;
  VERTEX *v ;
  FACE   *f ;
  
  for (i = vno = 0 ; vno < mris->nvertices ; vno++)
    {
      v = &mris->vertices[vno] ;
      if (v->ripflag) {
  i+=3;
  continue ;
      }
      vertices[i++] = -v->x ; vertices[i++] = v->z ; vertices[i++] = v->y ;
    }
  
  for (i = vno = 0 ; vno < mris->nvertices ; vno++)
    {
      v = &mris->vertices[vno] ;
      if (v->ripflag) {
  i+=3;
  continue ;
      }
      normals[i++] = -v->nx ; normals[i++] = v->nz ; normals[i++] = v->ny ;
    }
  
  for (i =  0; i<mris->nfaces*VERTICES_PER_FACE; i++)
    faces[i]=0;
  
  for (i = fno = 0 ; fno < mris->nfaces ; fno++)
    {
      f = &mris->faces[fno] ;
      if (f->ripflag)
  continue ;
      for (n = 0 ; n < VERTICES_PER_FACE ; n++)
  faces[i++] = f->v[n] ;
    }
  return(NO_ERROR) ;
}

static int
compile_brain_list(MRI_SURFACE *mris)
{
  int n,k;
  FACE *f;
  VERTEX* v;
  float curv;
  
  glDeleteLists(FS_Brain_List,1);
  glNewList(FS_Brain_List, GL_COMPILE);
  
  for (k=0;k<mris->nfaces;k++)
    if (!mris->faces[k].ripflag) {
      
      f = &mris->faces[k];
      bgnpolygon();
      for (n=0;n<VERTICES_PER_FACE;n++) {
  v = &mris->vertices[f->v[n]];
  if (flag2d && v->nz < 0)
    v->nz *= -1 ;
  /**** msurfer: single val data on gray curvature */
  if (overlayflag && !complexvalflag) {
    if (v->annotation) {
      RGBcolor(v->annotation,v->annotation,v->annotation);
    } else if (fieldsignflag) {
      if (v->fieldsign>0.0) {
        if (revphaseflag)
    set_color(v->fsmask*v->fieldsign,v->curv,FIELDSIGN_NEG);
        else
    set_color(v->fsmask*v->fieldsign,v->curv,FIELDSIGN_POS);
      } else {
        if (revphaseflag)
    set_color(-v->fsmask*v->fieldsign,v->curv,FIELDSIGN_POS);
        else
    set_color(-v->fsmask*v->fieldsign,v->curv,FIELDSIGN_NEG);
      }
    } else if (surfcolor)
      set_color(v->val,v->curv,REAL_VAL);
    else
      set_color(v->val,0.0,REAL_VAL);
    
    /**** msurfer: complex val data on gray curvature */
  } else if (overlayflag && complexvalflag) {
    if (surfcolor)
      set_complexval_color(v->val,v->val2,v->stat,v->curv);
    else
      set_complexval_color(v->val,v->val2,v->stat,0.0);
    
    /**** nsurfer: curvature, etc. red/green curv, 2d */
  } else {
    if (v->annotation) {
      int  r, g, b ;
      r = v->annotation & 0xff ;
      g = (v->annotation >> 8) & 0xff ;
      b = (v->annotation >> 16) & 0xff ;
      RGBcolor(r,g,b);
    } else {
      if (surfcolor==CURVATURE_OR_SULCUS)
        curv = (avgflag)?v->curv-dipavg:v->curv;
#if 0
      else if (surfcolor==AREAL_DISTORTION)
        curv = (avgflag)?v->logarat-logaratavg:v->logarat;
#endif
#if 0
      else if (surfcolor==SHEAR_DISTORTION)
        curv = (avgflag)?v->logshear-logshearavg:v->logshear; /* 2d only */
#endif
      else
        curv = 0.0;
      
      if (surfcolor)
        set_color(0.0,curv,GREEN_RED_CURV);
      else
        set_color(0.0,0.0,GREEN_RED_CURV);
      
      if (v->border)
        set_color(0.0,0.0,BORDER);
    }
  }
  
  if (v->marked)
    set_color(0.0,0.0,MARKED+v->marked-1);
  
  load_brain_coords(v->nx,v->ny,v->nz,v1);
  n3f(v1);
  load_brain_coords(v->x,v->y,v->z,v1);
  v3f(v1);
      }
      endpolygon();
    }
  glEndList();
  surface_compiled = 1;
  return(NO_ERROR) ; 
}

void
draw_surface(void)  /* marty: combined three versions */
{
  int k,n,m;
  FACE *f;
  VERTEX *v,*vnei;
  float curv;
  
  if (use_vertex_arrays) {
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glDeleteLists(FS_Brain_List,1);
    if (!colors)
      init_vertex_arrays(mris) ;
    if(vertex_array_dirty==1) {
      fill_vertex_arrays(mris);
      glEnableClientState ( GL_VERTEX_ARRAY );
      glEnableClientState ( GL_COLOR_ARRAY );
      glEnableClientState ( GL_NORMAL_ARRAY );
      glVertexPointer(3, GL_FLOAT, 0, vertices) ;
      glNormalPointer(GL_FLOAT, 0, normals) ;
      glColorPointer(3, GL_FLOAT, 0, colors);
      vertex_array_dirty = 0;
    }
    if(color_scale_changed) {
      fill_color_array(mris, colors) ;
      color_scale_changed = TRUE;
      glColorPointer  ( 3, GL_FLOAT, 0, colors ); 
    } 
    glPolygonOffset(1.0,1.0);
#ifndef IRIX
    glEnable(GL_POLYGON_OFFSET_FILL);
#endif
    /* Draw the object*/
    if(surfaceflag) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      if(VERTICES_PER_FACE==3) {
        glDrawElements ( GL_TRIANGLES, 3*mris->nfaces, GL_UNSIGNED_INT,faces );
      } else {
        glDrawElements ( GL_QUADS, 4*mris->nfaces, GL_UNSIGNED_INT, faces );
      }
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    if(pointsflag) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
      if(!mesh_colors)
        init_mesh_colors(mris);
      fill_mesh_colors();
      glColorPointer( 3, GL_FLOAT, 0, mesh_colors);
      if(VERTICES_PER_FACE==3) {
        glDrawElements ( GL_TRIANGLES, 3*mris->nfaces, GL_UNSIGNED_INT,faces );
      } else {
        glDrawElements ( GL_QUADS, 4*mris->nfaces, GL_UNSIGNED_INT, faces );
      }
    }
    if (verticesflag) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      if(!mesh_colors)
        init_mesh_colors(mris);
      fill_mesh_colors();
      glColorPointer( 3, GL_FLOAT, 0, mesh_colors);
      if(VERTICES_PER_FACE==3) {
        glDrawElements( GL_TRIANGLES, 3*mris->nfaces, GL_UNSIGNED_INT, faces );
      } else {
        glDrawElements ( GL_QUADS, 4*mris->nfaces, GL_UNSIGNED_INT, faces );
      }
    }
    /* not use vertex arrays */
  } else if(use_display_lists == 1 && use_vertex_arrays == 0) {
    printf(" using display lists !\n");
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    vertex_array_dirty = 1;/* vertex arrays might be destroyed at this point */
    if(!surface_compiled)
      compile_brain_list(mris);
    
    if(color_scale_changed) {
      compile_brain_list(mris);
      color_scale_changed = TRUE;
    }
    glPolygonOffset(1.0,1.0);
    glEnable(GL_POLYGON_OFFSET_FILL);
    /* Draw the object*/
    if(surfaceflag) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      glCallList(FS_Brain_List);
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    if(pointsflag) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
      glCallList(FS_Brain_List); 
    }
    if (verticesflag) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      glCallList(FS_Brain_List);
    }
  } else {
    if (surfaceflag) 
      {
  for (k=0;k<mris->nfaces;k++)
    if (!mris->faces[k].ripflag)
      {
        f = &mris->faces[k];
        bgnpolygon();
        for (n=0;n<VERTICES_PER_FACE;n++)
    {
      v = &mris->vertices[f->v[n]];
      if (flag2d && v->nz < 0)
        v->nz *= -1 ;
      /**** msurfer: single val data on gray curvature */
      if (overlayflag && !complexvalflag)
        {
          if (v->annotation)
      {
        RGBcolor(v->annotation,v->annotation,v->annotation);
      } 
          else if (fieldsignflag)
      {
        if (v->fieldsign>0.0) {
          if (revphaseflag)
            set_color(v->fsmask*v->fieldsign,v->curv,FIELDSIGN_NEG);
          else
            set_color(v->fsmask*v->fieldsign,v->curv,FIELDSIGN_POS);
        }
        else {
          if (revphaseflag)
            set_color(-v->fsmask*v->fieldsign,v->curv,FIELDSIGN_POS);
          else
            set_color(-v->fsmask*v->fieldsign,v->curv,FIELDSIGN_NEG);
        }
      } 
          else if (surfcolor)
      set_color(v->val,v->curv,REAL_VAL);
          else
      set_color(v->val,0.0,REAL_VAL);
        }
      
      /**** msurfer: complex val data on gray curvature */
      else if (overlayflag && complexvalflag)
        {
          if (surfcolor)
      set_complexval_color(v->val,v->val2,v->stat,v->curv);
          else
      set_complexval_color(v->val,v->val2,v->stat,0.0);
        }
      
      /**** nsurfer: curvature, etc. red/green curv, 2d */
      else
        {
          if (v->annotation)
      {
        int  r, g, b ;
        r = v->annotation & 0xff ;
        g = (v->annotation >> 8) & 0xff ;
        b = (v->annotation >> 16) & 0xff ;
        RGBcolor(r,g,b);
      }
          else
      {
        if (surfcolor==CURVATURE_OR_SULCUS)
          curv = (avgflag)?v->curv-dipavg:v->curv;
#if 0
        else if (surfcolor==AREAL_DISTORTION)
          curv = (avgflag)?v->logarat-logaratavg:v->logarat;
#endif
#if 0
        else if (surfcolor==SHEAR_DISTORTION)
          curv = (avgflag)?v->logshear-logshearavg:v->logshear; /* 2d only */
#endif
        else
          curv = 0.0;
        
        if (surfcolor)
          set_color(0.0,curv,GREEN_RED_CURV);
        else
          set_color(0.0,0.0,GREEN_RED_CURV);
        
        if (v->border)
          set_color(0.0,0.0,BORDER);
      }
        }
      
      if (v->marked)
        set_color(0.0,0.0,MARKED+v->marked-1);
      
      load_brain_coords(v->nx,v->ny,v->nz,v1);
      n3f(v1);
      load_brain_coords(v->x,v->y,v->z,v1);
      v3f(v1);
    }
        endpolygon();
      }
      }
    
    if (pointsflag) 
      {   /* with surface too, only points with z<0 drawn??! */
  RGBcolor(meshr,meshg,meshb);
  bgnpoint();
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag) 
      {
        v = &mris->vertices[k];
        load_brain_coords(v->nx,v->ny,v->nz,v1);
        n3f(v1);
        load_brain_coords(v->x+pup*v->nx,v->y+pup*v->ny,v->z+pup*v->nz,v1);
        v3f(v1);
      }
  endpoint();
      }
    
    if (verticesflag)  /* marty */
      {
  RGBcolor(meshr,meshg,meshb);
  for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag)
      {
        v = &mris->vertices[k];
        for (m=0;m<v->vnum;m++)
    {
      if (v->v[m]<k)      /* draw each edge once */
        if (!mris->vertices[v->v[m]].ripflag)
          {
      vnei = &mris->vertices[v->v[m]];
      linewidth(mesh_linewidth);
      bgnline();
      load_brain_coords(vnei->nx,vnei->ny,vnei->nz,v1);
      n3f(v1);
      load_brain_coords(vnei->x+mup*vnei->nx,vnei->y+mup*vnei->ny,
            vnei->z+mup*vnei->nz,v1);
      v3f(v1);
      load_brain_coords(v->nx,v->ny,v->nz,v1);
      n3f(v1);
      load_brain_coords(v->x+mup*v->nx,v->y+mup*v->ny,v->z+mup*v->nz,v1);
      v3f(v1);
      endline();
          }
    }
      }
      }
    
  }
  glFlush() ;
  if (scalebarflag)
    draw_scalebar();
  if (colscalebarflag)
    draw_colscalebar();
  glFlush() ;
}

static void
fill_color_array(MRI_SURFACE *mris, float *colors)
{
  int n;
  float curv;
  VERTEX *v;
#if VERTICES_PER_FACE == 4
  float intdiv0,intdiv1,intdiv2,intdiv3,frac0,frac1,frac2,frac3,nx,ny,nz;
  float cross_x[4],cross_y[4],cross_z[4];
  int crossnum;
#endif
  GLubyte  r, g, b ;
  /* begin rkt */
  GLubyte r_base, b_base, g_base;
  GLubyte r_overlay, b_overlay, g_overlay;
  float val;
  /* end rkt */
  
  for (n=0;n<mris->nvertices;n++)
    {
      v = &mris->vertices[n];
      

      /* begin rkt */
      if (simpledrawmodeflag)
  {
    /* if surfcolor (curvature) is on and there is an overlay,
       get a grayscale value. if just surfcolor and curvflag are
       on, get a red/green color based on the curvature. else
       just use the solid background color. */
    if (surfcolor && overlayflag && curvflag)
      {
        get_color_vals (0, v->curv, REAL_VAL,
            &r_base, &g_base, &b_base);
      } 
    else if (surfcolor && curvflag)
      {
        get_color_vals (0, v->curv, GREEN_RED_CURV, 
            &r_base, &g_base, &b_base);
      } else {
        get_color_vals (0, 0, REAL_VAL,  
            &r_base, &g_base, &b_base);
      }
    
    /* save the base color for later comparison, but set our
       final rgb values to it for now. */
    r = r_base;
    g = g_base;
    b = b_base;

    /* get any label color for this vertex. this will not apply
       any color if there is no label. */
    labl_apply_color_to_vertex (n, &r, &g, &b );
    
    /* if overlay flag is on... */
    if (overlayflag)
      {
        if (complexvalflag) 
    {
      /* if complexvalflag is on, we have to do this
         special drawing thing. this is for compatibility
         with the two-cond stuff. assumes that val, val2,
         and stat are loaded. */
      if (surfcolor)
        get_complexval_color_vals(v->val,v->val2,v->stat,v->curv,
              &r_overlay,&g_overlay,&b_overlay);
      else
        get_complexval_color_vals(v->val,v->val2,v->stat,0,
              &r_overlay,&g_overlay,&b_overlay);
      
    } else {
      /* get a red/green color based on the currently 
         selected field if it is above fthresh. */
      sclv_get_value (v, sclv_current_field, &val);
      if (val > fthresh || -val < -fthresh)
        {
          get_color_vals (val, v->curv, REAL_VAL,  
              &r, &g, &b);
          //              &r_overlay, &g_overlay, &b_overlay);
        }
    }

        /* if r1,g1,b1 is equal to the gray background,
     don't color anything, because we may cover up the
     label. if it's not, there's activation here, and we
     should cover the label. */
        /*
        if ( r_overlay != r_base || 
       g_overlay != g_base || 
       b_overlay != b_base ) {
    r = r_overlay;
    g = g_overlay;
    b = b_overlay;
        }
        */
      } 
    
    /* let the boundary code color this vertex, if it wants to. */
    fbnd_apply_color_to_vertex (n, &r, &g, &b);
    
  } else {
    /* end rkt */
    
    /**** msurfer: single val data on gray curvature */
    if (overlayflag && !complexvalflag)
      {
        if (v->annotation)
    {
      r = g = b = v->annotation ;
    } 
        else if (fieldsignflag)
    {
      if (v->fieldsign>0.0) 
        {
          if (revphaseflag)
      get_color_vals(v->fsmask*v->fieldsign,v->curv,FIELDSIGN_NEG,
               &r, &g, &b);
          else
      get_color_vals(v->fsmask*v->fieldsign,v->curv,FIELDSIGN_POS,
               &r, &g, &b);
        }
      else 
        {
          if (revphaseflag)
      get_color_vals(-v->fsmask*v->fieldsign,v->curv,FIELDSIGN_POS,
               &r, &g, &b);
          else
      get_color_vals(-v->fsmask*v->fieldsign,v->curv,FIELDSIGN_NEG,
               &r, &g, &b);
        }
    } 
        else if (surfcolor)
    get_color_vals(v->val,v->curv,REAL_VAL, &r, &g, &b);
        else
    get_color_vals(v->val,0.0,REAL_VAL, &r, &g, &b);
      }
    
    /**** msurfer: complex val data on gray curvature */
    else if (overlayflag && complexvalflag)
      {
        if (surfcolor)
    get_complexval_color_vals(v->val,v->val2,v->stat,v->curv, 
            &r, &g, &b);
        else
    get_complexval_color_vals(v->val,v->val2,v->stat,0.0, &r, &g, &b);
      }
    
    /**** nsurfer: curvature, etc. red/green curv, 2d */
    else
      {
        if (v->annotation)
    {
      /* int  r, g, b ; */
      r = v->annotation & 0xff ;
      g = (v->annotation >> 8) & 0xff ;
      b = (v->annotation >> 16) & 0xff ;
    }
        else
    {
      if (surfcolor==CURVATURE_OR_SULCUS)
        curv = (avgflag)?v->curv-dipavg:v->curv;
      else
        curv = 0.0;
      
      if (surfcolor) {
        get_color_vals(0.0,curv,GREEN_RED_CURV, &r, &g, &b);
      }        
      else
        get_color_vals(0.0,0.0,GREEN_RED_CURV, &r, &g, &b);
      
      if (v->border)
        get_color_vals(0.0,0.0,BORDER, &r, &g, &b);
    }
      }
  }
      
      if (v->marked)
  get_color_vals(0.0,0.0,MARKED+v->marked-1, &r, &g, &b);
      
      colors[3*n] = ((float)r)/255.0;
      colors[3*n+1] = ((float)g)/255.0;
      colors[3*n+2] = ((float)b)/255.0;
    }
}

static int
get_color_vals(float val, float curv, int mode,
         GLubyte *pr, GLubyte *pg, GLubyte *pb)
     
{
  short r,g,b;
  float f,fr,fg,fb,tmpoffset;
  
  val -= foffset ;
  
  r = g = b = 0 ;
  if (curv<0)  tmpoffset = cvfact*offset;
  else         tmpoffset = offset;
  
  if (mode==GREEN_RED_CURV)
    {
      if (curv < cmin)
  {
    b = 255 * (offset/blufact + 0.95*(1-offset/blufact)); /* yellow */
    r = g = 0 ;
    
  }
      else if (curv > cmax)
  {
    r = g = 255 * (offset/blufact + 0.95*(1-offset/blufact)); /* yellow */
    b = 0 ;
  }
      else
  {
    f = tanh(cslope*(curv-cmid));
    if (f>0) {
      r = 255 * (offset/blufact + 0.95*(1-offset/blufact)*fabs(f));
      g = 255 * (offset/blufact*(1 - fabs(f)));
    }
    else {
      r = 255 * (offset/blufact*(1 - fabs(f)));
      g = 255 * (offset/blufact + 0.95*(1-offset/blufact)*fabs(f));
    }
    b = 255 * (offset*blufact*(1 - fabs(f)));
  }
    }
  
  if (mode==REAL_VAL)   /* single val positive or signed */
    {
      if (colscale==HEAT_SCALE)  /* stat */
  {
    set_stat_color(val,&fr,&fg,&fb,tmpoffset);
    r=fr; g=fg; b=fb;
  }
      else  /* positive */
  if (colscale==CYAN_TO_RED || colscale==BLU_GRE_RED || colscale==JUST_GRAY)
    {
      if (val<fthresh) 
        {
    r = g = 255 * (tmpoffset/blufact);
    b =     255 * (tmpoffset*blufact);
        }
      else 
        {
    if (fslope!=0)
      f = (tanh(fslope*fmid)+tanh(fslope*(val-fmid)))/(2-tanh(fslope*fmid));
    else
      f = (val<0)?0:((val>1)?1:val);
    set_positive_color(f,&fr,&fg,&fb,tmpoffset);
    r=fr; g=fg; b=fb;
        }
    }
  else /* signed */
    {
      if (fabs(val)<fthresh) 
        {
    r = g = 255 * (tmpoffset/blufact);
    b =     255 * (tmpoffset*blufact);
        }
      else 
        {
    if (fslope!=0)
      {
        if (fmid==0)
          f = tanh(fslope*(val));
        else
          {
      if (val<0)
        f = -(tanh(fslope*fmid) + tanh(fslope*(-val-fmid)))/
          (2-tanh(fslope*fmid));
      else
        f = (tanh(fslope*fmid) + tanh(fslope*( val-fmid)))/
          (2-tanh(fslope*fmid));
          }
      }
    else
      f = (val<-1)?-1:((val>1)?1:val);
    if (revphaseflag)
      f = -f;
    if (truncphaseflag) {
      if (f<0.0) f = 0.0;
    }
    set_signed_color(f,&fr,&fg,&fb,tmpoffset);
    r=fr; g=fg; b=fb;
        }
    }
    }
  
  if (mode==FIELDSIGN_POS || mode==FIELDSIGN_NEG) {
    if (val<fthresh) {
      r = g = 255 * (tmpoffset/blufact);
      b =     255 * (tmpoffset*blufact);
    }
    else {
      f = (1.0 + tanh(fslope*(val-fmid)))/2.0;
      if (mode==FIELDSIGN_POS) {
        b = 255 * (tmpoffset + 0.95*(1-tmpoffset)*fabs(f));
        r = g = 255* (tmpoffset*(1 - fabs(f)));
      }
      else {
        b = 255 * (tmpoffset*(1 - fabs(f)));
        r = g = 255 * (tmpoffset + 0.95*(1-tmpoffset)*fabs(f));
      }
    }
  }
  
  if (mode==BORDER)  /* AMD 5/27/95 */
    {
      r = 255;
      g = 255;
      b = 0;
    }
  
  if (mode==MARKED)
    {
#if 0
      r = 255;
      g = 255;
      b = 255;
#else
      r = meshr;
      g = meshg;
      b = meshb;
#endif
    }
  if (mode > MARKED)
    {
      if (EVEN(mode-MARKED))
  {
    r = 255 ; g = 255 ; b = 0 ;
  }
      else
  {
    r = 0 ; g = 0 ; b = 255 ;
  }
    }
  
  r = (r<0)?0:(r>255)?255:r;
  g = (g<0)?0:(g>255)?255:g;
  b = (b<0)?0:(b>255)?255:b;
  *pr = (unsigned char)r ; *pg = (unsigned char)g ; *pb = (unsigned char)b ;
  return(NO_ERROR) ;
}

void
get_complexval_color_vals(float x, float y, float stat, float curv,
        GLubyte *pr, GLubyte *pg, GLubyte *pb)
{
  short sr,sg,sb;
  float f,a,r,g,b;
  float tmpoffset,fscale;
  float a_cycles, oa = 0.0f;
  
  stat -= foffset ;
  
  if (statflag || sclv_current_field == SCLV_VALSTAT)
    f = stat;
  else
    f = sqrt(x*x+y*y);
  
  if (curv<0.0) tmpoffset = cvfact*offset;
  else          tmpoffset = offset;
  
  if (fabs(f)<fthresh)  /* trunc */
    {
      r = g = 255 * (tmpoffset/blufact);
      b =     255 * (tmpoffset*blufact);
    }
  else  /* use complex (or stat vals which ignore complex!!) */
    {
      if (!statflag && sclv_current_field != SCLV_VALSTAT)
  {
    if (fslope!=0)
      f = (1.0 + tanh(fslope*(f-fmid)))/2.0;
    else
      f = (f<0)?0:((f>1)?1:f);
    
    if (truncphaseflag) 
      {
        a = atan2(y,x)/(2*M_PI);
        if (revphaseflag)
    a = -a;
        if (invphaseflag)
    a += 0.5;
        a -= angle_offset;
        a = fmod(a,1.0);
        if (a<0) a += 1;
        if (a>0.5)
    f = 0;
      }
  }
      
      fscale = f;
      
      if (colscale==HEAT_SCALE || colscale==CYAN_TO_RED ||
    colscale==BLU_GRE_RED || colscale==JUST_GRAY)
  {
    if (statflag || sclv_current_field == SCLV_VALSTAT)
      set_stat_color(f,&r,&g,&b,tmpoffset);
    else
      set_positive_color(f,&r,&g,&b,tmpoffset);
  }
      
      else if (colscale==TWOCOND_GREEN_RED)
  {
    a = atan2(y,x)/(2*M_PI);
    if (revphaseflag)
      a = -a;
    if (invphaseflag)
      a += 0.5;
    a -= angle_offset;       /* pos num cancels delay */
    a = fmod(a,1.0);         /* -1 to 1 */
    if (a<0) a += 1;         /* make positive */
    r = g = b = 0;
    f = sin(a*2*M_PI);
    if (f>0.0)
      r = 1;
    else
      g = 1;
    f = fabs(f)*fscale;
    r = 255 * (tmpoffset/blufact*(1-f)+f*r);
    g = 255 * (tmpoffset/blufact*(1-f)+f*g);
    b = 255 * (tmpoffset*blufact*(1-f));
  }
      
      else if (colscale==COLOR_WHEEL)
  {
    a = atan2(y,x)/(2*M_PI);
    if (revphaseflag)
      a = -a;
    if (invphaseflag)
      a += 0.5;
    a_cycles = angle_cycles;
    oa = a;
    
    if (fmod(angle_cycles,1.0)==0.0)  /* integral cycles (eccentricity) */
      {
        a -= angle_offset;
        a = fmod(a,1.0);
        if (a<0) a += 1;
        a -= 0.333333;           /* center on blue (1/3)*/
        a = a_cycles*a;          /* allow multiple */
        a = fmod(a,1.0);
        if (a<0) a += 1;
        a = 3*a;
        r = g = b = 0;
        if (a<1.0)
    {
      r = 1-a;
      b = a;
    }
        else if (a<2.0)
    {
      b = 1-(a-1);
      g = a-1;
    }
        else
    {
      r = a-2;
      g = 1-(a-2);
    }
      }
    else /* non-integral cycles (polar angle) */
      {
        a -= angle_offset;
        a = fmod(a,1.0);
        if (a<0) a += 1;
        a -= 0.5;          /* center on blue (1/2) */
        a = a_cycles*a;
        r = g = b = 0;
        if (a<-0.33333)
    {
      r = 1;
    }
        else if (a<0.0)
    {
      r = 1-(a-(-0.33333))/(0.0-(-0.33333));
      b = (a-(-0.33333))/(0.0-(-0.33333));
    }
        else if (a<0.33333)
    {
      b = 1-(a)/(0.33333);
      g = (a)/(0.33333);
    }
        else
    {
      g = 1;
    }
        
        if (a>fadef*a_cycles/2)
    {
      f = 1-(a-fadef*a_cycles/2)/(a_cycles/2-fadef*a_cycles/2);
      r = (tmpoffset*(1-f)+f*r);
      g = (tmpoffset*(1-f)+f*g);
      b = (tmpoffset*(1-f)+f*b);
    }
        if (a<-fadef*a_cycles/2)
    {
      f = (a-(-a_cycles/2))/(a_cycles/2-fadef*a_cycles/2);
      r = (tmpoffset*(1-f)+f*r);
      g = (tmpoffset*(1-f)+f*g);
      b = (tmpoffset*(1-f)+f*b);
    }
        
      } /* end non-integral */
    r = (tmpoffset*(1-fscale)+fscale*r)*255;
    b = (tmpoffset*(1-fscale)+fscale*b)*255;
    g = (tmpoffset*(1-fscale)+fscale*g)*255;
  }  /* end color wheel */
      
      else if (colscale==RYGB_WHEEL)
  {
    a = atan2(y,x)/(2*M_PI);
    if (revphaseflag)
      a = -a;
    if (invphaseflag)
      a += 0.5;
    a_cycles = angle_cycles;
    oa = a;
    
    a -= angle_offset;
    a = fmod(a,1.0);
    if (a<0) a += 1;
    a -= 0.25;           /* center on blue (1/4)*/
    a = a_cycles*a;          /* allow multiple */
    a = fmod(a,1.0);
    if (a<0) a += 1;
    a = 4*a;
    r = g = b = 0;
    if (a<1.0)
      {
        r = 1.0;
        g = a;
      }
    else if (a<2.0)
      {
        r = 1-(a-1);
        g = 1.0;
      }
    else if (a<3.0)
      {
        g = 1-(a-2);
        b = a-2;
      }
    else
      {
        r = a-3;
        b = 1-(a-3);
      }
    r = (tmpoffset*(1-fscale)+fscale*r)*255;
    b = (tmpoffset*(1-fscale)+fscale*b)*255;
    g = (tmpoffset*(1-fscale)+fscale*g)*255;
  }  /* end RYGB wheel */
      
      if (phasecontourflag) {
  if (phasecontour_min < phasecontour_max) {
    if (oa>phasecontour_min&&oa<phasecontour_max) {
      if (phasecontourmodflag)
        r = g = b = (tmpoffset*(1-fscale)+fscale*1.0)*255;
      else
        r = g = b = phasecontour_bright;
    }
  }
  else { /* wrap */
    if (oa>phasecontour_min||oa<phasecontour_max) {
      if (phasecontourmodflag)
        r = g = b = (tmpoffset*(1-fscale)+fscale*1.0)*255;
      else
        r = g = b = phasecontour_bright;
    }
  }
      }
    }
  sr = (r<0)?0:(r>255)?255:r;
  sg = (g<0)?0:(g>255)?255:g;
  sb = (b<0)?0:(b>255)?255:b;
  *pr = sr ; *pg = sg ; *pb = sb ;
}
#else
void
draw_surface(void)  /* marty: combined three versions */
{
  int k,n,m;
  FACE *f;
  VERTEX *v,*vnei;
  float curv;
#if VERTICES_PER_FACE == 4
  float intdiv0,intdiv1,intdiv2,intdiv3,frac0,frac1,frac2,frac3,nx,ny,nz;
  float cross_x[4],cross_y[4],cross_z[4];
  int crossnum;
#endif
  
  if (use_display_lists)
    {
      if (surface_compiled > 0)
  {
    glCallList(1) ;
    return ;
  }
      
      if (surface_compiled > -1)  /* needs recompilation - free old one */
  glDeleteLists(1, 1) ;
      glNewList(1, GL_COMPILE) ;
    }
  
#if 0
  glCullFace(GL_BACK);
  glEnable(GL_CULL_FACE);
#endif
  
  if (surfaceflag) {
    for (k=0;k<mris->nfaces;k++)
      if (!mris->faces[k].ripflag)
  {
    f = &mris->faces[k];
    bgnpolygon();
    for (n=0;n<VERTICES_PER_FACE;n++)
#if 0
      if (!flag2d || mris->vertices[f->v[n]].nz>0)
#endif
        {
    v = &mris->vertices[f->v[n]];
    if (flag2d && v->nz < 0)
      v->nz *= -1 ;
    
    /**** msurfer: single val data on gray curvature */
    if (overlayflag && !complexvalflag)
      {
        if (v->annotation)
          {
      RGBcolor(v->annotation,v->annotation,v->annotation);
          } 
        else if (fieldsignflag)
          {
      if (v->fieldsign>0.0) {
        if (revphaseflag)
          set_color(v->fsmask*v->fieldsign,v->curv,FIELDSIGN_NEG);
        else
          set_color(v->fsmask*v->fieldsign,v->curv,FIELDSIGN_POS);
      }
      else {
        if (revphaseflag)
          set_color(-v->fsmask*v->fieldsign,v->curv,FIELDSIGN_POS);
        else
          set_color(-v->fsmask*v->fieldsign,v->curv,FIELDSIGN_NEG);
      }
          } 
        else if (surfcolor)
          set_color(v->val,v->curv,REAL_VAL);
        else
          set_color(v->val,0.0,REAL_VAL);
      }
    
    /**** msurfer: complex val data on gray curvature */
    else if (overlayflag && complexvalflag)
      {
        if (surfcolor)
          set_complexval_color(v->val,v->val2,v->stat,v->curv);
        else
          set_complexval_color(v->val,v->val2,v->stat,0.0);
      }
    
    /**** nsurfer: curvature, etc. red/green curv, 2d */
    else
      {
        if (v->annotation)
          {
      int  r, g, b ;
      r = v->annotation & 0xff ;
      g = (v->annotation >> 8) & 0xff ;
      b = (v->annotation >> 16) & 0xff ;
      RGBcolor(r,g,b);
          }
        else
          {
      if (surfcolor==CURVATURE_OR_SULCUS)
        curv = (avgflag)?v->curv-dipavg:v->curv;
#if 0
      else if (surfcolor==AREAL_DISTORTION)
        curv = (avgflag)?v->logarat-logaratavg:v->logarat;
#endif
#if 0
      else if (surfcolor==SHEAR_DISTORTION)
        curv = (avgflag)?v->logshear-logshearavg:v->logshear; /* 2d only */
#endif
      else
        curv = 0.0;
      
      if (surfcolor)
        set_color(0.0,curv,GREEN_RED_CURV);
      else
        set_color(0.0,0.0,GREEN_RED_CURV);
      
      if (v->border)
        set_color(0.0,0.0,BORDER);
          }
      }
    
    if (v->marked)
      set_color(0.0,0.0,MARKED+v->marked-1);
    
    load_brain_coords(v->nx,v->ny,v->nz,v1);
    n3f(v1);
    load_brain_coords(v->x,v->y,v->z,v1);
    v3f(v1);
        }
    endpolygon();
  }
  }
  
  if (pointsflag) {   /* with surface too, only points with z<0 drawn??! */
    RGBcolor(meshr,meshg,meshb);
    bgnpoint();
    for (k=0;k<mris->nvertices;k++)
      if (!mris->vertices[k].ripflag) {
  v = &mris->vertices[k];
  load_brain_coords(v->nx,v->ny,v->nz,v1);
  n3f(v1);
  load_brain_coords(v->x+pup*v->nx,v->y+pup*v->ny,v->z+pup*v->nz,v1);
  v3f(v1);
      }
    endpoint();
  }
  
  if (verticesflag)  /* marty */
    {
      RGBcolor(meshr,meshg,meshb);
      for (k=0;k<mris->nvertices;k++)
  if (!mris->vertices[k].ripflag)
    {
      v = &mris->vertices[k];
      for (m=0;m<v->vnum;m++)
        {
    if (v->v[m]<k)      /* draw each edge once */
      if (!mris->vertices[v->v[m]].ripflag)
        {
          vnei = &mris->vertices[v->v[m]];
          linewidth(mesh_linewidth);
          bgnline();
          load_brain_coords(vnei->nx,vnei->ny,vnei->nz,v1);
          n3f(v1);
          load_brain_coords(vnei->x+mup*vnei->nx,vnei->y+mup*vnei->ny,
          vnei->z+mup*vnei->nz,v1);
          v3f(v1);
          load_brain_coords(v->nx,v->ny,v->nz,v1);
          n3f(v1);
          load_brain_coords(v->x+mup*v->nx,v->y+mup*v->ny,v->z+mup*v->nz,v1);
          v3f(v1);
          endline();
        }
        }
    }
    }
  
#if VERTICES_PER_FACE == 4
  /* don't know what to do here if VERTICES_PER_FACE is 3 */
  if (isocontourflag)
    {
      RGBcolor(meshr,meshg,meshb);
      for (k=0;k<mris->nfaces;k++)
  if (!mris->faces[k].ripflag)
    {
      f = &mris->faces[k];
      for (m=0;m<3;m++)
        {
    if (m==0)
      {
        RGBcolor(255,230,0);
        linewidth(4);
      }
    else if (m==1)
      {
        /*RGBcolor(190,145,255);*/
        RGBcolor(180,135,255);
        linewidth(3);
      }
    else if (m==2)
      RGBcolor(0,0,255);
    crossnum = 0;
    intdiv0 = floor(mris->vertices[f->v[0]].coords[m]/contour_spacing[m]);
    intdiv1 = floor(mris->vertices[f->v[1]].coords[m]/contour_spacing[m]);
    intdiv2 = floor(mris->vertices[f->v[2]].coords[m]/contour_spacing[m]);
    intdiv3 = floor(mris->vertices[f->v[3]].coords[m]/contour_spacing[m]);
    frac0   = mris->vertices[f->v[0]].coords[m]/contour_spacing[m]-intdiv0;
    frac1   = mris->vertices[f->v[1]].coords[m]/contour_spacing[m]-intdiv1;
    frac2   = mris->vertices[f->v[2]].coords[m]/contour_spacing[m]-intdiv2;
    frac3   = mris->vertices[f->v[3]].coords[m]/contour_spacing[m]-intdiv3;
    if (intdiv0!=intdiv1)
      {
        cross_x[crossnum] = mris->vertices[f->v[0]].x+
          (mris->vertices[f->v[1]].x-mris->vertices[f->v[0]].x)*
          (1-frac0)/(1+frac1-frac0);
        cross_y[crossnum] = mris->vertices[f->v[0]].y+
          (mris->vertices[f->v[1]].y-mris->vertices[f->v[0]].y)*
          (1-frac0)/(1+frac1-frac0);
        cross_z[crossnum] = mris->vertices[f->v[0]].z+
          (mris->vertices[f->v[1]].z-mris->vertices[f->v[0]].z)*
          (1-frac0)/(1+frac1-frac0);
        crossnum++;
      }
    if (intdiv1!=intdiv2)
      {
        cross_x[crossnum] = mris->vertices[f->v[1]].x+
          (mris->vertices[f->v[2]].x-mris->vertices[f->v[1]].x)*
          (1-frac1)/(1+frac2-frac1);
        cross_y[crossnum] = mris->vertices[f->v[1]].y+
          (mris->vertices[f->v[2]].y-mris->vertices[f->v[1]].y)*
          (1-frac1)/(1+frac2-frac1);
        cross_z[crossnum] = mris->vertices[f->v[1]].z+
          (mris->vertices[f->v[2]].z-mris->vertices[f->v[1]].z)*
          (1-frac1)/(1+frac2-frac1);
        crossnum++;
      }
    if (intdiv0!=intdiv2)
      {
        cross_x[crossnum] = mris->vertices[f->v[0]].x+
          (mris->vertices[f->v[2]].x-mris->vertices[f->v[0]].x)*
          (1-frac0)/(1+frac2-frac0);
        cross_y[crossnum] = mris->vertices[f->v[0]].y+
          (mris->vertices[f->v[2]].y-mris->vertices[f->v[0]].y)*
          (1-frac0)/(1+frac2-frac0);
        cross_z[crossnum] = mris->vertices[f->v[0]].z+
          (mris->vertices[f->v[2]].z-mris->vertices[f->v[0]].z)*
          (1-frac0)/(1+frac2-frac0);
        crossnum++;
      }
    if (intdiv0!=intdiv2)
      {
        cross_x[crossnum] = mris->vertices[f->v[0]].x+
          (mris->vertices[f->v[2]].x-mris->vertices[f->v[0]].x)*
          (1-frac0)/(1+frac2-frac0);
        cross_y[crossnum] = mris->vertices[f->v[0]].y+
          (mris->vertices[f->v[2]].y-mris->vertices[f->v[0]].y)*
          (1-frac0)/(1+frac2-frac0);
        cross_z[crossnum] = mris->vertices[f->v[0]].z+
          (mris->vertices[f->v[2]].z-mris->vertices[f->v[0]].z)*
          (1-frac0)/(1+frac2-frac0);
        crossnum++;
      }
    if (intdiv0!=intdiv3)
      {
        cross_x[crossnum] = mris->vertices[f->v[0]].x+
          (mris->vertices[f->v[3]].x-mris->vertices[f->v[0]].x)*
          (1-frac0)/(1+frac3-frac0);
        cross_y[crossnum] = mris->vertices[f->v[0]].y+
          (mris->vertices[f->v[3]].y-mris->vertices[f->v[0]].y)*
          (1-frac0)/(1+frac3-frac0);
        cross_z[crossnum] = mris->vertices[f->v[0]].z+
          (mris->vertices[f->v[3]].z-mris->vertices[f->v[0]].z)*
          (1-frac0)/(1+frac3-frac0);
        crossnum++;
      }
    if (intdiv3!=intdiv2)
      {
        cross_x[crossnum] = mris->vertices[f->v[3]].x+
          (mris->vertices[f->v[2]].x-mris->vertices[f->v[3]].x)*
          (1-frac3)/(1+frac2-frac3);
        cross_y[crossnum] = mris->vertices[f->v[3]].y+
          (mris->vertices[f->v[2]].y-mris->vertices[f->v[3]].y)*
          (1-frac3)/(1+frac2-frac3);
        cross_z[crossnum] = mris->vertices[f->v[3]].z+
          (mris->vertices[f->v[2]].z-mris->vertices[f->v[3]].z)*
          (1-frac3)/(1+frac2-frac3);
        crossnum++;
      }
    if (crossnum>0 && fabs(intdiv0-intdiv1)<=1
        && fabs(intdiv1-intdiv2)<=1
        && fabs(intdiv0-intdiv2)<=1
        && fabs(intdiv0-intdiv3)<=1
        && fabs(intdiv3-intdiv2)<=1)
      {
        nx = (mris->vertices[f->v[0]].nx+mris->vertices[f->v[1]].nx+
        mris->vertices[f->v[2]].nx)/VERTICES_PER_FACE;
        ny = (mris->vertices[f->v[0]].ny+mris->vertices[f->v[1]].ny+
        mris->vertices[f->v[2]].ny)/VERTICES_PER_FACE;
        nz = (mris->vertices[f->v[0]].nz+mris->vertices[f->v[1]].nz+
        mris->vertices[f->v[2]].nz)/VERTICES_PER_FACE;
        if (crossnum!=2 && crossnum!=4)
          {
      printf("surfer: error - crossnum=%d\n",crossnum);
          }
        else
          {
      bgnline();
      load_brain_coords(nx,ny,nz,v1);
      n3f(v1);
      load_brain_coords(cross_x[0]+mup*nx,cross_y[0]+mup*ny,
            cross_z[0]+mup*nz,v1);
      v3f(v1);
      load_brain_coords(cross_x[1]+mup*nx,cross_y[1]+mup*ny,
            cross_z[1]+mup*nz,v1);
      v3f(v1);
      endline();
      if (crossnum>2)
        {
          bgnline();
          load_brain_coords(nx,ny,nz,v1);
          n3f(v1);
          load_brain_coords(cross_x[2]+mup*nx,cross_y[2]+mup*ny,
                cross_z[2]+mup*nz,v1);
          v3f(v1);
          load_brain_coords(cross_x[3]+mup*nx,cross_y[3]+mup*ny,
                cross_z[3]+mup*nz,v1);
          v3f(v1);
          endline();
        }
          }
      }
        }
    }
    }
#endif
  
  if (flag2d)  /* additional line annotations (2d only) */
    {
#if 0
      if (shearvecflag)
  for (k=0;k<mris->nvertices;k++)
    if ((k%10)==0)
      if (!mris->vertices[k].ripflag)
        {
    v = &mris->vertices[k];
    linewidth(1);
    bgnline();
    RGBcolor(255,255,255);
    load_brain_coords(v->nx,v->ny,v->nz,v1);
    n3f(v1);
    load_brain_coords(v->x-2.00*v->shearx,v->y-2.00*v->sheary,v->z,v2);
    load_brain_coords(v->x+2.00*v->shearx,v->y+2.00*v->sheary,v->z,v3);
    v3f(v2);
    v3f(v3);
    endline();
        }
#endif
#if 0
      if (normvecflag)
  for (k=0;k<mris->nvertices;k++)
    if ((!mris->vertices[k].ripflag)&&mris->vertices[k].border)
      {
        v = &mris->vertices[k];
        linewidth(1);
        bgnline();
        RGBcolor(0,255,0);
        load_brain_coords(v->nx,v->ny,v->nz,v1);
        n3f(v1);
        load_brain_coords(v->x,v->y,v->z,v2);
        load_brain_coords(v->x+2.00*v->bnx,v->y+2.00*v->bny,v->z,v3);
        v3f(v2);
        v3f(v3);
        endline();
      }
#endif
#if 0
      if (movevecflag)
  for (k=0;k<mris->nvertices;k++)
    if ((!mris->vertices[k].ripflag)&&(mris->vertices[k].border||((k%10)==0)))
      {
        v = &mris->vertices[k];
        linewidth(1);
        bgnline();
        if (mris->vertices[k].border)
    RGBcolor(255,0,255);
        else
    RGBcolor(0,255,255);
        load_brain_coords(v->nx,v->ny,v->nz,v1);
        n3f(v1);
        load_brain_coords(v->x,v->y,v->z,v2);
        load_brain_coords(v->x+5.00*v->odx,v->y+5.00*v->ody,v->z,v3);
        v3f(v2);
        v3f(v3);
        endline();
      }
#endif
    }
  if (scalebarflag)
    draw_scalebar();
  if (colscalebarflag)
    draw_colscalebar();
  if (use_display_lists)
    {
      glEndList() ;
      surface_compiled = 1 ;
      glCallList(1) ;
    }
}
#endif

void
draw_ellipsoid_latlong(float a, float b, float c)   /* 50.0,140.0,80.0 */
{
#ifdef OPENGL
  /* omit this (6 gl calls found here only) */
#else
  VERTEX *v;
  int k;
  float x,y,z;
  float cx,cy,cz;
  float lati,longi;
  
  a *= 1.01;  /* brain x,  50, lhrh     */
  b *= 1.01;  /* brain y, 140  ant/post */
  c *= 1.01;  /* brain z,  80, sup/inf  */
  cx = cy = cz = 0.0;
  for (k=0;k<mris->nvertices;k++) {
    v = &mris->vertices[k];
    cx += v->x;
    cy += v->y;
    cz += v->z;
  }
  cx /= mris->nvertices;
  cy /= mris->nvertices;
  cz /= mris->nvertices;
  printf("surfer: ellipsoid center: x=%f y=%f z=%f\n",cx,cy,cz);
  
  blendfunction(BF_SA, BF_MSA);  /* no antialias despite these 4 lines */
  linesmooth(SML_ON);
  setlinestyle(0xFFFF);
  lsrepeat(1);
  pushmatrix();    /* circ() draws in brain transverse plane */
  translate(-cx,cz,cy);  /* center:cx->lh/rh, cz->sup/inf, cy->ant/post */
  for (lati= -90.0; lati<90.0; lati+=10.0) {
    pushmatrix();
    translate(0.0,0.0,b*sin(M_PI/180.0*lati));
    scale(a/c,1.0,1.0);
    if (lati==0.0) { linewidth(6); RGBcolor(255,255,0); }
    else           { linewidth(2); RGBcolor(217,170,0); }
    circ(0.0,0.0,c*cos(M_PI/180.0*lati));
    popmatrix();
  }
  RGBcolor(190,145,255);
  linewidth(2);
  for (longi=0.0; longi<180.0; longi+=10.0) {
    pushmatrix();
    rot(90.0,'y');
    scale(1.0,c/b,a/b);
    rot(longi,'x');
    circ(0.0,0.0,b);
    popmatrix();
  }
  popmatrix();
  linesmooth(SML_OFF);
#endif
}

void
draw_second_surface(void)   /* for blink doublebuffer--FIX: no 2d */
{
  int k,n;
  FACE *f;
  VERTEX *v;
  float curv;
  
  if (flag2d) {printf("surfer: ### can't yet blink flat\n");PR return;}
  
  for (k=0;k<mris2->nfaces;k++)
    if (!mris2->faces[k].ripflag)
      {
  f = &mris2->faces[k];
  bgnpolygon();
  for (n=0;n<VERTICES_PER_FACE;n++)
    if (!flag2d || mris2->vertices[f->v[n]].nz>0)
      {
        v = &mris2->vertices[f->v[n]];
        if (surfcolor==CURVATURE_OR_SULCUS)
    curv = (avgflag)?v->curv-dipavg2:v->curv;
        else
    curv = 0.0;
        if (surfcolor)
    set_color(0.0,curv,GREEN_RED_CURV);
        else
    set_color(0.0,0.0,GREEN_RED_CURV);
        if (v->border)
    set_color(0.0,0.0,BORDER);
        load_brain_coords(v->nx,v->ny,v->nz,v1);
        n3f(v1);
        load_brain_coords(v->x,v->y,v->z,v1);
        v3f(v1);
      }
  endpolygon();
      }
}

void
draw_scalebar(void)
{
  float v[3], tmpzf;
  
  pushmatrix();
  tmpzf = zf;  /* push zf */
  linewidth(SCALEBAR_WIDTH);
  RGBcolor(scalebar_bright,scalebar_bright,scalebar_bright);
  restore_zero_position();  /* zf => 1.0 */
  scale_brain(tmpzf);
  bgnline();
  v[0] = fov*sf*scalebar_xpos/tmpzf;
  v[1] = -fov*sf*scalebar_ypos/tmpzf;
  v[2] = fov*sf*9.99/tmpzf;
  v3f(v);
  v[0] -= SCALEBAR_MM;
  v3f(v);
  endline();
  popmatrix();
  zf = tmpzf;
}

void
draw_colscalebar(void)
{
  int i;
  float v[3], tmpzf, stat, maxval;
  int NSEGMENTS = 100;
  
  maxval = fmid+1.0/fslope;
  pushmatrix();
  tmpzf = zf;  /* push zf */
  restore_zero_position();  /* zf => 1.0 */
  v[0] = v[1] = 0;
  v[2] = 1.0;
  n3f(v);
  for (i=0;i<NSEGMENTS-1;i++)
    {
      /*
  stat = fthresh+i*(maxval-fthresh)/(NSEGMENTS-1.0);
      */
      stat = -maxval+i*(2*maxval)/(NSEGMENTS-1.0)+foffset;
      if (statflag || sclv_current_field == SCLV_VALSTAT)
        set_complexval_color(0.0,0.0,stat,0.0);
      else
      {
        if (complexvalflag)
          set_complexval_color(stat,0.0,0.0,0.0);
        else
          set_color(stat,0.0,REAL_VAL);
      }
      bgnquadrangle() ;
      v[0] = fov*sf*(colscalebar_xpos-colscalebar_width/2);
      v[1] =
        fov*sf*(colscalebar_ypos+colscalebar_height*(i/(NSEGMENTS-1.0)-0.5));
      v[2] = fov*sf*9.99;
      v3f(v);
      v[0] = fov*sf*(colscalebar_xpos+colscalebar_width/2);
      v3f(v);
      v[1] = 
        fov*sf*(colscalebar_ypos+colscalebar_height*((i+1)/(NSEGMENTS-1.0)-0.5));
      v3f(v);
      v[0] = fov*sf*(colscalebar_xpos-colscalebar_width/2);
      v3f(v);
      endpolygon();
    }
  popmatrix();
  zf = tmpzf;
}

void
set_stat_color(float f, float *rp, float *gp, float *bp, float tmpoffset)
{
  float r,g,b;
  float ftmp,c1,c2;
  
  r = g = b = 0.0f ;
  if (invphaseflag)
    f = -f;
  if (truncphaseflag && f<0)
    f = 0;
  if (rectphaseflag)
    f = fabs(f);
  
  if (fabs(f)>fthresh && fabs(f)<fmid)
    {
      ftmp = fabs(f);
      c1 = 1.0/(fmid-fthresh);
      if (fcurv!=1.0)
  c2 = (fmid-fthresh-fcurv*c1*SQR(fmid-fthresh))/
    ((1-fcurv)*(fmid-fthresh));
      else
  c2 = 0;
      ftmp = fcurv*c1*SQR(ftmp-fthresh)+c2*(1-fcurv)*(ftmp-fthresh)+fthresh;
      f = (f<0)?-ftmp:ftmp;
    }
  
  if (colscale==HEAT_SCALE)
    {
      if (f>=0)
  {
    r = tmpoffset*((f<fthresh)?1:(f<fmid)?1-(f-fthresh)/(fmid-fthresh):0) +
      ((f<fthresh)?0:(f<fmid)?(f-fthresh)/(fmid-fthresh):1);
    g = tmpoffset*((f<fthresh)?1:(f<fmid)?1-(f-fthresh)/(fmid-fthresh):0) +
      ((f<fmid)?0:(f<fmid+1.00/fslope)?1*(f-fmid)*fslope:1);
    b = tmpoffset*((f<fthresh)?1:(f<fmid)?1-(f-fthresh)/(fmid-fthresh):0);
  } else
    {
      f = -f;
      b = tmpoffset*((f<fthresh)?1:(f<fmid)?1-(f-fthresh)/(fmid-fthresh):0) +
        ((f<fthresh)?0:(f<fmid)?(f-fthresh)/(fmid-fthresh):1);
      g = tmpoffset*((f<fthresh)?1:(f<fmid)?1-(f-fthresh)/(fmid-fthresh):0) +
        ((f<fmid)?0:(f<fmid+1.00/fslope)?1*(f-fmid)*fslope:1);
      r = tmpoffset*((f<fthresh)?1:(f<fmid)?1-(f-fthresh)/(fmid-fthresh):0);
    }
      r = r*255;
      g = g*255;
      b = b*255;
    }
  else if (colscale==BLU_GRE_RED)
    {
      /*
  if (f<0) f = -f;
  b = tmpoffset*((f<fthresh)?1:(f<fmid)?1-(f-fthresh)/(fmid-fthresh):0) +
        ((f<fthresh)?0:(f<fmid)?(f-fthresh)/(fmid-fthresh):
  (f<fmid+0.25/fslope)?1-4*(f-fmid)*fslope:
  (f<fmid+0.75/fslope)?0:
  (f<fmid+1.00/fslope)?4*(f-(fmid+0.75/fslope))*fslope:1);
    g = tmpoffset*((f<fthresh)?1:(f<fmid)?1-(f-fthresh)/(fmid-fthresh):0) +
    ((f<fmid)?0:(f<fmid+0.25/fslope)?4*(f-fmid)*fslope:
    (f<fmid+0.50/fslope)?1-4*(f-(fmid+0.25/fslope))*fslope:
    (f<fmid+0.75/fslope)?4*(f-(fmid+0.50/fslope))*fslope:1);
    r = tmpoffset*((f<fthresh)?1:(f<fmid)?1-(f-fthresh)/(fmid-fthresh):0) +
         ((f<fmid+0.25/fslope)?0:(f<fmid+0.50/fslope)?4*(f-(fmid+0.25/fslope))
   *fslope:1);
   */
    if (f>=0)
    {
      r = tmpoffset*((f<fthresh)?1:(f<fmid)?1-(f-fthresh)/(fmid-fthresh):0) +
  ((f<fthresh)?0:(f<fmid)?(f-fthresh)/(fmid-fthresh):1);
      g = tmpoffset*((f<fthresh)?1:(f<fmid)?1-(f-fthresh)/(fmid-fthresh):0) +
  ((f<fmid)?0:(f<fmid+1.00/fslope)?1*(f-fmid)*fslope:1);
      b = tmpoffset*((f<fthresh)?1:(f<fmid)?1-(f-fthresh)/(fmid-fthresh):0) +
  ((f<fmid)?0:(f<fmid+1.00/fslope)?1*(f-fmid)*fslope:1);
    } else
      {
  f = -f;
  b = tmpoffset*((f<fthresh)?1:(f<fmid)?1-(f-fthresh)/(fmid-fthresh):0) +
    ((f<fthresh)?0:(f<fmid)?(f-fthresh)/(fmid-fthresh):1);
  g = tmpoffset*((f<fthresh)?1:(f<fmid)?1-(f-fthresh)/(fmid-fthresh):0) +
          ((f<fmid)?0:(f<fmid+1.00/fslope)?1*(f-fmid)*fslope:1);
  r = tmpoffset*((f<fthresh)?1:(f<fmid)?1-(f-fthresh)/(fmid-fthresh):0) +
    ((f<fmid)?0:(f<fmid+1.00/fslope)?1*(f-fmid)*fslope:1);
      }
    r = r*255;
    g = g*255;
    b = b*255;
    }
  else if (colscale==JUST_GRAY)
  {
    if (f<0) f = -f;
    r = g = b = f*255;
  }
  *rp = r;
  *gp = g;
  *bp = b;
}

void
set_positive_color(float f, float *rp, float *gp, float *bp, float tmpoffset)
{
  float r,g,b;

  f -= foffset ;
  r = g = b = 0 ;
  f = fabs(f);
  if (colscale==HEAT_SCALE)
  {
    r = tmpoffset + f*(1-tmpoffset);
    g = tmpoffset + ((f>0.333)?(f-0.333)/(1.0-0.333):0)*(1-tmpoffset);
    b = tmpoffset + ((f>0.666)?(f-0.666)/(1.0-0.666):0)*(1-tmpoffset);
    r = r*255;
    g = g*255;
    b = b*255;
  }
  else if (colscale==CYAN_TO_RED)
  {
    b = g = tmpoffset + (1-tmpoffset)*((f<0.3)?f:0.3*(1-(f-0.3)/(1-0.3)));
    r = tmpoffset + (1-tmpoffset)*((f>0.3)?(f-0.3)/(1-0.3):0);
    r = r*255;
    g = g*255;
    b = b*255;
  }
  else if (colscale==BLU_GRE_RED)
  {
    b = tmpoffset + (1-tmpoffset) *
        ((f<0.25)?f:((f<0.50)?(0.25)*(1-(f-0.25)/(1-0.25)):0));
    g = tmpoffset + (1-tmpoffset) *
      ((f<0.25)?0:((f<0.50)?2*(f-0.25):2*(0.50-0.25)*(1-(f-0.50)/(1-0.50))));
    r = tmpoffset + (1-tmpoffset)*((f<0.50)?0:(f-0.50)/(1-0.50));
    r = r*255;
    g = g*255;
    b = b*255;
  }
  else if (colscale==JUST_GRAY)
  {
    r = g = b = f*255;
  }
  *rp = r;
  *gp = g;
  *bp = b;
}

void
set_signed_color(float f, float *rp, float *gp, float *bp, float tmpoffset)
{
  float r,g,b;

  if (colscale==BLUE_TO_RED_SIGNED)
  {
    b = tmpoffset + (1-tmpoffset) *
        ((f<0)?-2*f:(f>0.75)?((0.5)*(f-0.75)/(1-0.75)):0);
    r = tmpoffset + (1-tmpoffset) *
        ((f>0)?2*f:(f<-0.75)?((0.5)*(-f-0.75)/(1-0.75)):0);
    g = tmpoffset + (1-tmpoffset) *
      ((f<-0.5)?(-f-0.5)/(1.0-0.5):
       (f>0.50)?(f-0.5)/(1.0-0.5):0);
    r = r*255;
    g = g*255;
    b = b*255;
  } else
  if (colscale==GREEN_TO_RED_SIGNED)
  {
    b = tmpoffset + (1-tmpoffset) *
        ((f<-0.5)?(1-(-f-0.5)/(1-0.5)):(f<0.0)?(-f/0.5):
         (f>0.5)?(1-(f-0.5)/(1-0.5)):(f/0.5));
    r = tmpoffset + (1-tmpoffset) * ((f>0)?2*f:0);
    g = tmpoffset + (1-tmpoffset) * ((f<0)?-2*f:0);
    r = r*255;
    g = g*255;
    b = b*255;
  } else  /*  */
  {
    if (f>0) {
      r = 255 * (tmpoffset/blufact + 0.95*(1-tmpoffset/blufact)*fabs(f));
      g = 255 * (tmpoffset/blufact*(1 - fabs(f)));
    }
    else {
      r = 255 * (tmpoffset/blufact*(1 - fabs(f)));
      g = 255 * (tmpoffset/blufact + 0.95*(1-tmpoffset/blufact)*fabs(f));
    }
    b = 255 * (tmpoffset*blufact*(1 - fabs(f)));
  }
  *rp = r;
  *gp = g;
  *bp = b;
}

void
set_color(float val, float curv, int mode)
{
  short r,g,b;
  float f,fr,fg,fb,tmpoffset;

  val -= foffset ;

  r = g = b = 0 ;
  if (curv<0)  tmpoffset = cvfact*offset;
  else         tmpoffset = offset;

  if (mode==GREEN_RED_CURV)
  {
    if (curv < cmin)
    {
      b = 255 * (offset/blufact + 0.95*(1-offset/blufact)); /* yellow */
      r = g = 0 ;
      
    }
    else if (curv > cmax)
    {
      r = g = 255 * (offset/blufact + 0.95*(1-offset/blufact)); /* yellow */
      b = 0 ;
    }
    else
    {
      f = tanh(cslope*(curv-cmid));
      if (f>0) {
        r = 255 * (offset/blufact + 0.95*(1-offset/blufact)*fabs(f));
        g = 255 * (offset/blufact*(1 - fabs(f)));
      }
      else {
        r = 255 * (offset/blufact*(1 - fabs(f)));
        g = 255 * (offset/blufact + 0.95*(1-offset/blufact)*fabs(f));
      }
      b = 255 * (offset*blufact*(1 - fabs(f)));
    }
  }

  if (mode==REAL_VAL)   /* single val positive or signed */
  {
    if (colscale==HEAT_SCALE)  /* stat */
    {
      set_stat_color(val,&fr,&fg,&fb,tmpoffset);
      r=fr; g=fg; b=fb;
    }
    else  /* positive */
    if (colscale==CYAN_TO_RED || colscale==BLU_GRE_RED || colscale==JUST_GRAY)
    {
      if (val<fthresh) 
      {
        r = g = 255 * (tmpoffset/blufact);
        b =     255 * (tmpoffset*blufact);
      }
      else 
      {
        if (fslope!=0)
          f = (tanh(fslope*fmid)+tanh(fslope*(val-fmid)))/(2-tanh(fslope*fmid));
        else
          f = (val<0)?0:((val>1)?1:val);
        set_positive_color(f,&fr,&fg,&fb,tmpoffset);
        r=fr; g=fg; b=fb;
      }
    }
    else /* signed */
    {
      if (fabs(val)<fthresh) 
      {
        r = g = 255 * (tmpoffset/blufact);
        b =     255 * (tmpoffset*blufact);
      }
      else 
      {
        if (fslope!=0)
        {
          if (fmid==0)
            f = tanh(fslope*(val));
          else
          {
            if (val<0)
              f = -(tanh(fslope*fmid) + tanh(fslope*(-val-fmid)))/
                   (2-tanh(fslope*fmid));
            else
              f = (tanh(fslope*fmid) + tanh(fslope*( val-fmid)))/
                  (2-tanh(fslope*fmid));
          }
        }
        else
          f = (val<-1)?-1:((val>1)?1:val);
        if (revphaseflag)
          f = -f;
        if (truncphaseflag) {
          if (f<0.0) f = 0.0;
        }
        set_signed_color(f,&fr,&fg,&fb,tmpoffset);
        r=fr; g=fg; b=fb;
      }
    }
  }

  if (mode==FIELDSIGN_POS || mode==FIELDSIGN_NEG) {
    if (val<fthresh) {
      r = g = 255 * (tmpoffset/blufact);
      b =     255 * (tmpoffset*blufact);
    }
    else {
      f = (1.0 + tanh(fslope*(val-fmid)))/2.0;
      if (mode==FIELDSIGN_POS) {
        b = 255 * (tmpoffset + 0.95*(1-tmpoffset)*fabs(f));
        r = g = 255* (tmpoffset*(1 - fabs(f)));
      }
      else {
        b = 255 * (tmpoffset*(1 - fabs(f)));
        r = g = 255 * (tmpoffset + 0.95*(1-tmpoffset)*fabs(f));
      }
    }
  }

  if (mode==BORDER)  /* AMD 5/27/95 */
  {
    r = 255;
    g = 255;
    b = 0;
  }

  if (mode==MARKED)
  {
#if 0
    r = 255;
    g = 255;
    b = 255;
#else
    r = meshr;
    g = meshg;
    b = meshb;
#endif
  }
  if (mode > MARKED)
  {
    if (EVEN(mode-MARKED))
    {
      r = 255 ; g = 255 ; b = 0 ;
    }
    else
    {
      r = 0 ; g = 0 ; b = 255 ;
    }
  }

  r = (r<0)?0:(r>255)?255:r;
  g = (g<0)?0:(g>255)?255:g;
  b = (b<0)?0:(b>255)?255:b;
  RGBcolor(r,g,b);
}

void
set_complexval_color(float x, float y, float stat, float curv)
{
  short sr,sg,sb;
  float f,a,r,g,b;
  float tmpoffset,fscale;
  float a_cycles, oa = 0.0f;

  f -= foffset ;

  if (statflag || sclv_current_field == SCLV_VALSTAT)
    f = stat;
  else
    f = sqrt(x*x+y*y);

  if (curv<0.0) tmpoffset = cvfact*offset;
  else          tmpoffset = offset;

  if (fabs(f)<fthresh)  /* trunc */
  {
    r = g = 255 * (tmpoffset/blufact);
    b =     255 * (tmpoffset*blufact);
  }
  else  /* use complex (or stat vals which ignore complex!!) */
  {
    if (!statflag && (sclv_current_field != SCLV_VALSTAT))
    {
      if (fslope!=0)
        f = (1.0 + tanh(fslope*(f-fmid)))/2.0;
      else
        f = (f<0)?0:((f>1)?1:f);

      if (truncphaseflag) 
      {
        a = atan2(y,x)/(2*M_PI);
        if (revphaseflag)
          a = -a;
        if (invphaseflag)
          a += 0.5;
        a -= angle_offset;
        a = fmod(a,1.0);
        if (a<0) a += 1;
        if (a>0.5)
          f = 0;
      }
    }

    fscale = f;

    if (colscale==HEAT_SCALE || colscale==CYAN_TO_RED ||
        colscale==BLU_GRE_RED || colscale==JUST_GRAY)
    {
      if (statflag || sclv_current_field == SCLV_VALSTAT)
        set_stat_color(f,&r,&g,&b,tmpoffset);
      else
        set_positive_color(f,&r,&g,&b,tmpoffset);
    }

    else if (colscale==TWOCOND_GREEN_RED)
    {
      a = atan2(y,x)/(2*M_PI);
      if (revphaseflag)
        a = -a;
      if (invphaseflag)
        a += 0.5;
      a -= angle_offset;       /* pos num cancels delay */
      a = fmod(a,1.0);         /* -1 to 1 */
      if (a<0) a += 1;         /* make positive */
      r = g = b = 0;
      f = sin(a*2*M_PI);
      if (f>0.0)
        r = 1;
      else
        g = 1;
      f = fabs(f)*fscale;
      r = 255 * (tmpoffset/blufact*(1-f)+f*r);
      g = 255 * (tmpoffset/blufact*(1-f)+f*g);
      b = 255 * (tmpoffset*blufact*(1-f));
    }

    else if (colscale==COLOR_WHEEL)
    {
      a = atan2(y,x)/(2*M_PI);
      if (revphaseflag)
        a = -a;
      if (invphaseflag)
        a += 0.5;
      a_cycles = angle_cycles;
      oa = a;

      if (fmod(angle_cycles,1.0)==0.0)  /* integral cycles (eccentricity) */
      {
        a -= angle_offset;
        a = fmod(a,1.0);
        if (a<0) a += 1;
        a -= 0.333333;           /* center on blue (1/3)*/
        a = a_cycles*a;          /* allow multiple */
        a = fmod(a,1.0);
        if (a<0) a += 1;
        a = 3*a;
        r = g = b = 0;
        if (a<1.0)
        {
          r = 1-a;
          b = a;
        }
        else if (a<2.0)
        {
          b = 1-(a-1);
          g = a-1;
        }
        else
        {
          r = a-2;
          g = 1-(a-2);
        }
      }
      else /* non-integral cycles (polar angle) */
      {
        a -= angle_offset;
        a = fmod(a,1.0);
        if (a<0) a += 1;
        a -= 0.5;          /* center on blue (1/2) */
        a = a_cycles*a;
        r = g = b = 0;
        if (a<-0.33333)
        {
          r = 1;
        }
        else if (a<0.0)
        {
          r = 1-(a-(-0.33333))/(0.0-(-0.33333));
          b = (a-(-0.33333))/(0.0-(-0.33333));
        }
        else if (a<0.33333)
        {
          b = 1-(a)/(0.33333);
          g = (a)/(0.33333);
        }
        else
        {
          g = 1;
        }

        if (a>fadef*a_cycles/2)
        {
           f = 1-(a-fadef*a_cycles/2)/(a_cycles/2-fadef*a_cycles/2);
           r = (tmpoffset*(1-f)+f*r);
           g = (tmpoffset*(1-f)+f*g);
           b = (tmpoffset*(1-f)+f*b);
        }
        if (a<-fadef*a_cycles/2)
        {
           f = (a-(-a_cycles/2))/(a_cycles/2-fadef*a_cycles/2);
           r = (tmpoffset*(1-f)+f*r);
           g = (tmpoffset*(1-f)+f*g);
           b = (tmpoffset*(1-f)+f*b);
        }

      } /* end non-integral */
      r = (tmpoffset*(1-fscale)+fscale*r)*255;
      b = (tmpoffset*(1-fscale)+fscale*b)*255;
      g = (tmpoffset*(1-fscale)+fscale*g)*255;
    }  /* end color wheel */

    else if (colscale==RYGB_WHEEL)
    {
      a = atan2(y,x)/(2*M_PI);
      if (revphaseflag)
        a = -a;
      if (invphaseflag)
        a += 0.5;
      a_cycles = angle_cycles;
      oa = a;

      a -= angle_offset;
      a = fmod(a,1.0);
      if (a<0) a += 1;
      a -= 0.25;           /* center on blue (1/4)*/
      a = a_cycles*a;          /* allow multiple */
      a = fmod(a,1.0);
      if (a<0) a += 1;
      a = 4*a;
      r = g = b = 0;
      if (a<1.0)
      {
        r = 1.0;
        g = a;
      }
      else if (a<2.0)
      {
        r = 1-(a-1);
        g = 1.0;
      }
      else if (a<3.0)
      {
        g = 1-(a-2);
        b = a-2;
      }
      else
      {
        r = a-3;
        b = 1-(a-3);
      }
      r = (tmpoffset*(1-fscale)+fscale*r)*255;
      b = (tmpoffset*(1-fscale)+fscale*b)*255;
      g = (tmpoffset*(1-fscale)+fscale*g)*255;
    }  /* end RYGB wheel */

    if (phasecontourflag) {
      if (phasecontour_min < phasecontour_max) {
        if (oa>phasecontour_min&&oa<phasecontour_max) {
          if (phasecontourmodflag)
            r = g = b = (tmpoffset*(1-fscale)+fscale*1.0)*255;
          else
            r = g = b = phasecontour_bright;
        }
      }
      else { /* wrap */
        if (oa>phasecontour_min||oa<phasecontour_max) {
          if (phasecontourmodflag)
            r = g = b = (tmpoffset*(1-fscale)+fscale*1.0)*255;
          else
            r = g = b = phasecontour_bright;
        }
      }
    }
  }
  sr = (r<0)?0:(r>255)?255:r;
  sg = (g<0)?0:(g>255)?255:g;
  sb = (b<0)?0:(b>255)?255:b;
  RGBcolor(sr,sg,sb);
}

void
draw_spokes(int option)
{
  float /*r0=0.1*/r0=0.07,r1=1.0,th0=0,th1=2*M_PI;
  int nr=100,nth=100;
  float scf=50.0;
  int thi,ri;
  double cosa,sina,cosb,sinb,ra,rb,tha,thb,a;
  float v0[3],v1[3],v2[3],v3[3],v4[3];

  RGBcolor(0,0,0);
  czclear(BACKGROUND,getgconfig(GC_ZMAX));
  v0[2] = v1[2] = v2[2] = v3[2] = 0;
  v4[0] = v4[1] = 0;
  v4[2] = 1;
  a = pow(r1/r0,1.0/(r1-r0)); /* marty */
  for (thi=0;thi<nth-1;thi++)
  {
    tha = th0+thi*(th1-th0)/(nth-1);
    thb = th0+(thi+1)*(th1-th0)/(nth-1);
    cosa = cos(tha);
    sina = sin(tha);
    cosb = cos(thb);
    sinb = sin(thb);
    for (ri=1;ri<nr+1;ri++)
    {
      ra = r0+(ri-1)*(r1-r0)/(nr);
      rb = r0+(ri)*(r1-r0)/(nr);
      ra = (ra<r0)?r0:(ra>r1)?r1:ra;
      rb = (rb<r0)?r0:(rb>r1)?r1:rb;
      if (option==RADIUS) /* marty: just change size */
      {
        ra = r0*pow(a,ra-r0);
        rb = r0*pow(a,rb-r0);
      }
      v0[0] = cosa*ra*scf;
      v0[1] = sina*ra*scf;
      v1[0] = cosa*rb*scf;
      v1[1] = sina*rb*scf;
      v2[0] = cosb*rb*scf;
      v2[1] = sinb*rb*scf;
      v3[0] = cosb*ra*scf;
      v3[1] = sinb*ra*scf;
      n3f(v4);
      bgnquadrangle();
      set_vertex_color((ri-1.0)/(nr),(thi-0.0)/(nth-1.0),option);
      v3f(v0);
      set_vertex_color((ri-0.0)/(nr),(thi-0.0)/(nth-1.0),option);
      v3f(v1);
      set_vertex_color((ri-0.0)/(nr),(thi+1.0)/(nth-1.0),option);
      v3f(v2);
      set_vertex_color((ri-1.0)/(nr),(thi+1.0)/(nth-1.0),option);
      v3f(v3);
      endpolygon();
    }
  }
}

void
set_vertex_color(float r, float th, int option)
{
  if (option==RADIUS) set_color_wheel(r,0.0,angle_cycles,0,TRUE,1.0);
  if (option==THETA)  set_color_wheel(th,0.0,angle_cycles,0,FALSE,1.0);
}

    /* msurferCMF.c (msurferMARTY.c hack) */
void
set_color_wheel(float a, float a_offset, float a_cycles, int mode, int logmode,
                float fscale) 
{
  float r,g,b,f,fadef=0.7;
  short sr,sg,sb;
  float maxrad=1.0,minrad=0.05,afact,offset=0.0f;

  if (mode==-1) offset=cvfact*offset;
  else          offset=offset;
  afact = pow(maxrad/minrad,1.0/(maxrad-minrad));
  if (fmod(a_cycles,1.0)==0)
  {
    a += 0.5;
    a += a_offset;
    a = fmod(a,1.0);
    if (logmode)
      a += 0.5;  /* marty: color center of half-duty ring */
      /*a = (log(a/minrad)/log(afact)+minrad-minrad)/(1-minrad); as below */
      /*a = minrad*pow(afact,a-minrad); */
    a = fmod(a,1.0);  /* marty: OK if not logmode?? */
    a = 3*a;
    r = g = b = 0;
    if (a<1.0)
    {
      r = 1-a;
      b = a;
    } else
    if (a<2.0)
    {
      b = 1-(a-1);
      g = a-1;
    } else
    {
      r = a-2;
      g = 1-(a-2);
    }
  }
  else  /* with angle offset */
  {
    a += 0.5;
    a += a_offset;
    a = fmod(a,1.0);
    a -= 0.5;
    a = a_cycles*a;
    /*a = (a<-1)?-1:(a>1)?1:a;*/
    r = g = b = 0;
    if (a<-0.5)
    {
      r = 1;
    } else
    if (a<0.0)
    {
      r = 1-(a-(-0.5))/(0.0-(-0.5));
      b = (a-(-0.5))/(0.0-(-0.5));
    } else
    if (a<0.5)
    {
      b = 1-(a)/(0.5);
      g = (a)/(0.5);
    } else
    {
      g = 1;
    }
    if (a>fadef*a_cycles/2)
    {
       f = 1-(a-fadef*a_cycles/2)/(a_cycles/2-fadef*a_cycles/2);
/*
       r *= f;
       g *= f;
       b *= f;
*/
       r = (offset*(1-f)+f*r);
       g = (offset*(1-f)+f*g);
       b = (offset*(1-f)+f*b);
    }
    if (a<-fadef*a_cycles/2)
    {
       f = (a-(-a_cycles/2))/(a_cycles/2-fadef*a_cycles/2);
       /*r *= f;g *= f;b *= f;*/
       r = (offset*(1-f)+f*r);
       g = (offset*(1-f)+f*g);
       b = (offset*(1-f)+f*b);
    }
  }
  /*r = fscale*r*255;b = fscale*b*255;g = fscale*g*255;*/
  r = (offset*(1-fscale)+fscale*r)*255;
  b = (offset*(1-fscale)+fscale*b)*255;
  g = (offset*(1-fscale)+fscale*g)*255;
  sr = (r<0)?0:(r>255)?255:r;
  sg = (g<0)?0:(g>255)?255:g;
  sb = (b<0)?0:(b>255)?255:b;
  RGBcolor(sr,sg,sb);
}

void
restore_ripflags(int mode)
{
  int k,h;

  printf("restore_ripflags(%d)\n",mode);
  if (mode==1)
  for (k=0;k<mris->nvertices;k++)
  {
    h = mris->vertices[k].oripflag;
    mris->vertices[k].oripflag = mris->vertices[k].ripflag;
    mris->vertices[k].ripflag = h;
  }
  else if (mode==2)
  for (k=0;k<mris->nvertices;k++)
  {
    mris->vertices[k].oripflag = mris->vertices[k].ripflag;
    mris->vertices[k].ripflag = mris->vertices[k].origripflag;
  }
  rip_faces();
  for (k=0;k<mris->nvertices;k++)
    mris->vertices[k].marked = FALSE;
  clear_vertex_marks();
  vertex_array_dirty = 1;
}

void
floodfill_marked_patch(int filltype)
{
  VERTEX *v, *vn;
  int i,k,m,filled,totfilled=0,kmin,kmax,kstep;
  /* begin rkt */
  float value;
  /* end rkt */

  if (nmarked==0) return;
  fprintf(stderr, "floodfill_marked_patch(%d)\n", filltype) ;
  if (area)
    LabelFree(&area) ;
    
  if (filltype == CURVFILL)
  {
    area = LabelAlloc(MAXMARKED, pname, lfname) ;
    fprintf(stderr, "subject_name=%s, lfname=%s\n", pname,lfname) ;
    LabelCurvFill(area, marked, nmarked, MAXMARKED, mris) ;
#if 0
    LabelWrite(area, lfname) ;
    LabelFree(&area) ;
#endif
    LabelMark(area, mris) ;
    /*    redraw() ;*/
    return ;
  }
  else if (filltype == RIPFILL)
  {
    area = LabelAlloc(MAXMARKED, pname, lfname) ;
    fprintf(stderr, "subject_name=%s, lfname=%s\n", pname,lfname) ;
    LabelFillAll(area, marked, nmarked, MAXMARKED, mris) ;
#if 0
    LabelWrite(area, lfname) ;
    LabelFree(&area) ;
#endif
    LabelMark(area, mris) ;

    /* begin rkt */
    undo_begin_action (UNDO_CUT);
    /* end rkt */

    for (k=0;k<mris->nvertices;k++)
    {
      v = &mris->vertices[k] ;
      if (!v->marked)
        /* begin rkt */
        /*v->ripflag = 1 ;*/
        set_vertex_rip (k, TRUE, TRUE);
      /* end rkt */
      else
        /* begin rkt */
        /*v->marked = 0 ;*/   /* unmark it so curvature will show */
        set_vertex_rip (k, FALSE, TRUE);
      /* end rkt */
    }
    rip_faces() ;
    
    /* begin rkt */
    undo_finish_action ();
    /* end rkt */
    
    redraw() ;
    return ;
  }
    

  filled = nmarked;
  i = 0;
  while (filled>0)
  {
    filled = 0;
    if ((i%2)==0)
    {
      kmin = 0;
      kmax = mris->nvertices-1;
      kstep = 1;
    } else
    {
      kmin = mris->nvertices-1;
      kmax = 0;
      kstep = -1;
    }
    for (k=kmin;k!=kmax;k+=kstep)
    {
      if (mris->vertices[k].marked)   /* look for an unmarked neighbor */
      {
        v = &mris->vertices[k];
        for (m=0;m<v->vnum;m++)
        {
          vn = &mris->vertices[v->v[m]] ;
/* begin rkt */
#if 0
          if (!vn->marked&&
              !vn->border&&
              (filltype!=STATFILL || fabs(vn->stat)>=fthresh))
#else
            sclv_get_value(vn,sclv_current_field,&value);
          if (!vn->marked&&
              !vn->border&&
              (filltype!=STATFILL || fabs(value)>=fthresh))
#endif
/* end rkt */
          {
            vn->marked = TRUE;
            filled++;
            totfilled++;
          }
        }
      }
    }
    printf("surfer: %d: filled = %d, total = %d (of %d)\n",
           i,filled,totfilled,mris->nvertices);
    draw_surface();
    i++;
  }
  area = LabelFromMarkedSurface(mris) ;
  PR

#if 0
#if 1
  MRISclearMarks(mris) ; nmarked = 0 ;
#else
  for (k=0;k<mris->nvertices;k++)
    mris->vertices[k].marked = FALSE;
  clear_vertex_marks();
#endif
#endif
}

void
clear_ripflags(void)
{
  int k;

  /* begin rkt */
  undo_begin_action (UNDO_CUT);

  for (k=0;k<mris->nvertices;k++)
    /* mris->vertices[k].ripflag = FALSE;*/
    set_vertex_rip (k, FALSE, TRUE);

  undo_finish_action();
  /* end rkt */

  rip_faces();
}

void
cut_marked_vertices(int closedcurveflag)
{
  int i,j,k,m,mmin;
  float d,d1,d2,dx,dy,dz,x1,y1,z1,x2,y2,z2,dmin;
  VERTEX *v1,*v2;

  /* begin rkt */
  printf ("******************** cut_marked_vertices called\n");
  undo_begin_action (UNDO_CUT);
  /* end rkt */

  for (i=0;i<((closedcurveflag)?nmarked:nmarked-1);i++)
  {
    j = (i==nmarked-1)?0:i+1;
    v2 = &mris->vertices[marked[j]];
    x2 = v2->x;
    y2 = v2->y;
    z2 = v2->z;
    k = marked[i];
    while (k!=marked[j])
    {
      v1 = &mris->vertices[k];
      v1->border = 1;
      /* begin rkt */
      /*v1->ripflag = 1;*/
      set_vertex_rip (k, TRUE, TRUE);
      /* end rkt */
      x1 = v1->x;
      y1 = v1->y;
      z1 = v1->z;
      dmin = 100000;
      mmin = 0;
      for (m=0;m<v1->vnum;m++)
      if (!mris->vertices[v1->v[m]].border||v1->v[m]==marked[0])
      {
        dx = mris->vertices[v1->v[m]].x - x1;
        dy = mris->vertices[v1->v[m]].y - y1;
        dz = mris->vertices[v1->v[m]].z - z1;
        d1 = sqrt(dx*dx+dy*dy+dz*dz);
        dx = mris->vertices[v1->v[m]].x - x2;
        dy = mris->vertices[v1->v[m]].y - y2;
        dz = mris->vertices[v1->v[m]].z - z2;
        d2 = sqrt(dx*dx+dy*dy+dz*dz);
        if ((d=d1+d2)<dmin) 
        {
          dmin = d;
          mmin = m;
        }
        printf("surfer:       k=%d, m=%d, v1->v[m]=%d, d1=%f, d2=%f, d=%f\n",
               k,m,v1->v[m],d1,d2,d);
      }
      if (mmin==100000) {printf("surfer: failed (infinite loop)\n");PR return;}
      k = v1->v[mmin];
      printf("surfer: best: mmin=%d, k=%d, dmin=%f\n",mmin,k,dmin);
    }
    /* begin rkt */
    /*v2->border = 1;*/
    /*v2->ripflag = 1;*/
    set_vertex_rip (marked[j], TRUE, TRUE);
    /* end rkt */
  }
  PR
  rip_faces();

  /* begin rkt */
  undo_begin_action (UNDO_CUT);
  /* end rkt */

  clear_vertex_marks();
}

void
cut_plane(void)
{
  int k;
  float dx0,dy0,dz0,dx1,dy1,dz1,nx,ny,nz,poffset,sign;
  VERTEX *v,*v0,*v1,*v2,*v3;

  /* begin rkt */
  undo_begin_action (UNDO_CUT);
  /* end rkt */

  if (nmarked!=4) {printf("surfer: needs 4 marked vertices\n");PR return;}
  /* begin rkt */
#if 0
  for (k=0;k<mris->nvertices;k++)
    mris->vertices[k].oripflag = mris->vertices[k].ripflag;
#endif
  /* end rkt */
  v0 = &mris->vertices[marked[0]];
  v1 = &mris->vertices[marked[1]];
  v2 = &mris->vertices[marked[2]];
  v3 = &mris->vertices[marked[3]];
  dx0 = v1->x-v0->x;
  dy0 = v1->y-v0->y;
  dz0 = v1->z-v0->z;
  dx1 = v2->x-v1->x;
  dy1 = v2->y-v1->y;
  dz1 = v2->z-v1->z;
  nx = -dy1*dz0 + dy0*dz1;
  ny = dx1*dz0 - dx0*dz1;
  nz = -dx1*dy0 + dx0*dy1;
  poffset = nx*v0->x+ny*v0->y+nz*v0->z; 
  sign = (nx*v3->x+ny*v3->y+nz*v3->z)-poffset;
  printf("surfer: poffset = %f, sign = %f, n = {%f,%f,%f}\n",
            poffset,sign,nx,ny,nz);PR
  for (k=0;k<mris->nvertices;k++)
  {
    v = &mris->vertices[k];
    if ((((nx*v->x+ny*v->y+nz*v->z)-poffset)/sign)<0)
      /* begin rkt */
      /*v->ripflag = TRUE;*/
      set_vertex_rip (k, TRUE, TRUE);
    /* end rkt */
  }
  rip_faces();
  clear_vertex_marks();
  vertex_array_dirty = 1;

  /* begin rkt */
  undo_finish_action();
  /* end rkt */
}

void
cut_vertex(void)
{
  /* begin rkt */
# if 0
  int k;

  undo_begin_action (UNDO_CUT);

  for (k=0;k<mris->nvertices;k++)
    mris->vertices[k].oripflag = mris->vertices[k].ripflag;
  mris->vertices[marked[nmarked-1]].ripflag = TRUE;
#endif
  set_vertex_rip (marked[nmarked-1], TRUE, TRUE);
  rip_faces();
  clear_vertex_marks();

  undo_finish_action();
  /* end rkt */
}

void
cut_line(int closedcurveflag)
{
  int i,j,k,m;
  float dx0,dy0,dz0,dx1,dy1,dz1,nx,ny,nz,x1,y1,z1,x2,y2,z2;
  float f1,f2,d,d0,d1,a,x,y,z,xi,yi,zi,xj,yj,zj,s1,s2;
  VERTEX *v1,*v2,*vi,*vj;

  /* begin rkt */
  undo_begin_action (UNDO_CUT);
  /* finish rkt  */

  if (nmarked<2) {
    printf("surfer: needs at least 2 marked vertices\n");PR return;}
  /* begin rkt */
#if 0
  for (k=0;k<mris->nvertices;k++)
    mris->vertices[k].oripflag = mris->vertices[k].ripflag;
#endif
  /* end rkt */
  if (closedcurveflag)
  {
    marked[nmarked] = marked[0];
    nmarked++;
  }
  for (i=0;i<nmarked-1;i++)
  {
    printf("surfer: i=%d\n",i);
    j = i+1;
    vi = &mris->vertices[marked[i]];
    vj = &mris->vertices[marked[j]];
    xi = vi->x;
    yi = vi->y;
    zi = vi->z;
    xj = vj->x;
    yj = vj->y;
    zj = vj->z;
    dx0 = vj->x-vi->x;
    dy0 = vj->y-vi->y;
    dz0 = vj->z-vi->z;
    dx1 = vi->nx+vj->nx;
    dy1 = vi->ny+vj->ny;
    dz1 = vi->nz+vj->nz;
    nx = -dy1*dz0 + dy0*dz1;
    ny = dx1*dz0 - dx0*dz1;
    nz = -dx1*dy0 + dx0*dy1;
    d = sqrt(nx*nx+ny*ny+nz*nz);
    nx /= d; ny /= d; nz /= d;
    d0 = sqrt(dx0*dx0+dy0*dy0+dz0*dz0);
    dx0 /= d0; dy0 /= d0; dz0 /= d0;
    d1 = sqrt(dx1*dx1+dy1*dy1+dz1*dz1);
    dx1 /= d1; dy1 /= d1; dz1 /= d1;
    for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag) 
    {
      v1 = &mris->vertices[k];
      x1 = v1->x - xi;
      y1 = v1->y - yi;
      z1 = v1->z - zi;
      s1 = nx*x1+ny*y1+nz*z1;
      for (m=0;m<v1->vnum;m++)
      if (!mris->vertices[v1->v[m]].ripflag)
      {
        v2 = &mris->vertices[v1->v[m]];
        x2 = v2->x - xi;
        y2 = v2->y - yi;
        z2 = v2->z - zi;
        s2 = nx*x2+ny*y2+nz*z2;
        if (s1*s2<=0)
        {
          a = s2/(s2-s1);
          x = a*x1+(1-a)*x2;
          y = a*y1+(1-a)*y2;
          z = a*z1+(1-a)*z2;
          f1 = x*dx0+y*dy0+z*dz0;
          f2 = x*dx1+y*dy1+z*dz1;
          if (f1>=-1&&f1<=d0+1&&f2>=-5.0&&f2<=5.0)
          {
            printf("surfer: delete connection (%d to %d)\n",k,v1->v[m]);
            printf("surfer: {%f,%f,%f} - {%f,%f,%f}\nf1=%f, f2=%f\n",
                   x1,y1,z1,x2,y2,z2,f1,f2);
      /* begin rkt */
            /*v1->ripflag = TRUE;*/
      set_vertex_rip (k, TRUE, TRUE);
      /* end rkt */
          }
        }
      }
    }
  }
  PR
  rip_faces();
  
  clear_vertex_marks();
  vertex_array_dirty = 1;

  /* begin rkt */
  undo_finish_action();
  /* finish rkt  */
}

/* begin rkt */

void set_face_rip(int fno, int rip, int undoable)
{
  FACE *f;

  f = &mris->faces[fno];

  /* add the action to undo list if the values are different */
  if (undoable && rip!=f->ripflag)
      undo_new_action_cut (UNDO_CUT_FACE, fno, f->ripflag);

  f->ripflag = rip;
}

void set_vertex_rip(int vno, int rip, int undoable)
{
  VERTEX *v;

  v = &mris->vertices[vno];

  /* add the action to undo list if the values are different */
  if (undoable && rip!=v->ripflag)
      undo_new_action_cut (UNDO_CUT_VERTEX, vno, v->ripflag);

  v->ripflag = rip;

}

void close_marked_vertices () 
{
  /* just add a copy of the first vertex to the end of the list. */
  marked[nmarked] = marked[0];
  nmarked++;
}

void draw_vertex_hilite (int vno)
{
  VERTEX* v;
  VERTEX* vn;
  int neighbor_index;

  if (vno < 0 || vno >= mris->nvertices)
    return;
  
  v = &(mris->vertices[vno]);
  
  for (neighbor_index = 0; neighbor_index < v->vnum; neighbor_index++)
    {
      vn = &(mris->vertices[v->v[neighbor_index]]);

      glLineWidth (CURSOR_LINE_PIX_WIDTH);

      glBegin (GL_LINES);

      glNormal3f (-(vn->nx), vn->nz, vn->ny);

      glVertex3f (-(vn->x + cup * vn->nx),
      vn->z + cup * vn->nz,
      vn->y + cup * vn->ny);

      glNormal3f (-(v->nx), v->nz, v->ny);

      glVertex3f (-(v->x + cup * vn->nx),
      v->z + cup * vn->nz,
      v->y + cup * vn->ny);
      glEnd ();
    }

  glFlush ();
}

void draw_marked_vertices ()
{
  int marked_index;

  /* draw all our marked verts in white. */
  RGBcolor (255, 255, 255);
  for (marked_index = 0; marked_index < nmarked-1; marked_index++)
    draw_vertex_hilite (marked[marked_index]);
}

/* end rkt */

void
draw_vector(char *fname)
{
  float   dx, dy, len ;
  float   v[3] ;
  FILE    *fp ;
  static int ncalls = 0 ;

  fp = fopen(fname, "r") ;

  if (!fp)
  {
    printf("could not open file %s.\n", fname) ;
    return ;
  }

  fscanf(fp, "%f %f", &dx, &dy) ;
  len = sqrt(dx*dx + dy*dy) ;
  dx /= len ; dy /= len ;

  if ((ncalls++ % 2) == 0)
    RGBcolor(0, 255, 255) ;
  else
    RGBcolor(255, 255, 0) ;
  linewidth(CURSOR_LINE_PIX_WIDTH);
  bgnline() ;
  load_brain_coords(0,0,1,v);
  n3f(v);

  load_brain_coords(0,0,0,v);
  v3f(v);
  load_brain_coords(dx*10,dy*10,0,v);
  v3f(v);

  endline() ;
  fclose(fp) ;
}

void 
put_retinotopy_stats_in_vals(void)
{
  int      vno ;
  VERTEX   *v ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    v->val = hypot(v->val, v->val2) ;
  }
}

void
plot_marked(char *fname)
{
  FILE   *fp ;
  int    vno, total ;
  VERTEX *v ;

  fprintf(stderr, "generating data file %s with entries:\n", fname) ;
  fprintf(stderr, "vno x y z distance curv val val2 stat amp "
          "deg normalized_deg radians\n") ;
  fp = fopen(fname, "w") ;
  if (!fp)
  {
    fprintf(stderr, "### could not open file %s\n", fname) ;
    return ;
  }

  for (vno = total = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag || !v->marked)
      continue ;
    fprintf(fp, "%d %f %f %f %f %f %f %f %f %f %f %f %f\n",
            vno, v->x, v->y, v->z, 0.0, v->curv, v->val, v->val2, v->stat,
            hypot(v->val,v->val2), (float)(atan2(v->val2,v->val)*180/M_PI),
            (float)(atan2(v->val2,v->val)/(2*M_PI)), 
            (float)(atan2(v->val2,v->val))) ;
                    
  }
  fclose(fp) ;
}

static int find_best_path(MRI_SURFACE *mris, int start_vno, int end_vno,
                          int *path_indices, double target_curv, double l_curv, 
                          double l_len) ;

void
draw_fundus(int bdry_index)
{
  FBND_BOUNDARY  *borig, *bnew, *bold ;
  double         last_sse, sse, curv_error, initial_length, total_length, 
                 l_curv, l_len, min_curv_error, target_curv ;
  int            i, vno, n, min_n, *indices/*, new_index, old_index*/, v_in_path,
                 start_vno, end_vno, niter ;
  VERTEX         *v = NULL, *v_last, *vn ;

  MRISclearMarks(mris) ;
  target_curv = mris->max_curv ;
  if (bdry_index == FBND_NONE_SELECTED)
  {
    printf("### no boundary selected\n") ;
    return ;
  }
  borig = &fbnd_boundaries[bdry_index] ; v_last = NULL ;
  l_curv = 1.0 ; l_len = 0.01 ;
  initial_length = fbnd_length(borig, mris) ;

  bnew = (FBND_BOUNDARY *)calloc(1, sizeof(FBND_BOUNDARY)) ;
  bnew->vertices = (int *)calloc(mris->nvertices, sizeof(int)) ;
  bold = fbnd_copy(borig, NULL) ;
  indices = (int *)calloc(mris->nvertices, sizeof(int)) ;
  if (indices == NULL || bnew->vertices == NULL)
    ErrorExit(ERROR_NOMEMORY, "%s: could not allocate index array", Progname) ;
  fbnd_copy(bold, bnew) ;
  niter = 0 ;
  do
  {
    /*    fbnd_set_marks(bold, mris, 1) ;*/

    /* compute sse of current line */
    last_sse = fbnd_sse(bold, mris, target_curv, l_curv, l_len) ;
    
    bnew->num_vertices = 0 ;

    /* first move endpoints in direction of max curvature */
    fbnd_set_marks(bold, mris, 1) ;
    start_vno = bold->vertices[0] ;
    min_n = -1 ; 
    min_curv_error = v->curv-target_curv ; min_curv_error *= min_curv_error ;
    v = &mris->vertices[start_vno] ;
    for (n = 0 ; n < v->vnum ; n++)
    {
      vn = &mris->vertices[v->v[n]] ;
      if (vn->marked)
        continue ;  /* already in path */
      curv_error = vn->curv - target_curv ; curv_error *= curv_error ;
      if (min_n < 0 || curv_error < min_curv_error)
      {
        min_curv_error = curv_error ;
        min_n = n ;
      }
    }
    if (min_n >= 0)
    {
      printf("modifying starting vertex to be %d (was %d)...\n", 
             v->v[min_n],start_vno) ;
      start_vno = v->v[min_n] ;
    }
    else   /* try searching nbrs of nbr of endpoint */
    {
      min_n = -1 ; 
      v = &mris->vertices[bold->vertices[0]] ;
      min_curv_error = v->curv-target_curv ; min_curv_error *= min_curv_error ;
      v = &mris->vertices[bold->vertices[1]] ;
      for (n = 0 ; n < v->vnum ; n++)
      {
        vn = &mris->vertices[v->v[n]] ;
        if (vn->marked)
          continue ;  /* already in path */
        curv_error = vn->curv - target_curv ; curv_error *= curv_error ;
        if (min_n < 0 || curv_error < min_curv_error)
        {
          min_curv_error = curv_error ;
          min_n = n ;
        }
      }
      if (min_n >= 0)
      {
        printf("modifying starting vertex to be %d (was %d)...\n", 
               v->v[min_n],start_vno) ;
        start_vno = v->v[min_n] ;
      }
    }

    end_vno = bold->vertices[bold->num_vertices-1];
    v = &mris->vertices[end_vno] ;
    min_n = -1 ; 
    min_curv_error = v->curv-target_curv ; min_curv_error *= min_curv_error ;
    for (n = 0 ; n < v->vnum ; n++)
    {
      vn = &mris->vertices[v->v[n]] ;
      if (vn->marked)
        continue ;  /* already in path */
      curv_error = vn->curv - target_curv ; curv_error *= curv_error ;
      if (min_n < 0 || curv_error < min_curv_error)
      {
        min_curv_error = curv_error ;
        min_n = n ;
      }
    }
    if (min_n >= 0)
    {
      printf("modifying end point to be %d (was %d)...\n", v->v[min_n], end_vno) ;
      end_vno = v->v[min_n] ;
    }
    else   /* try searching nbrs of nbr of endpoint */
    {
      min_n = -1 ; 
      v = &mris->vertices[bold->vertices[bold->num_vertices-1]] ;
      min_curv_error = v->curv-target_curv ; min_curv_error *= min_curv_error ;
      v = &mris->vertices[bold->vertices[bold->num_vertices-2]] ;
      for (n = 0 ; n < v->vnum ; n++)
      {
        vn = &mris->vertices[v->v[n]] ;
        if (vn->marked)
          continue ;  /* already in path */
        curv_error = vn->curv - target_curv ; curv_error *= curv_error ;
        if (min_n < 0 || curv_error < min_curv_error)
        {
          min_curv_error = curv_error ;
          min_n = n ;
        }
      }
      if (min_n >= 0)
      {
        printf("modifying ending vertex to be %d (was %d)...\n", 
               v->v[min_n],end_vno) ;
        end_vno = v->v[min_n] ;
      }
    }
    

    /* consider removing this vertex and putting a new path in */
    v_in_path = find_best_path(mris, start_vno, end_vno, indices,
                               target_curv, l_curv, l_len) ;
    /* indices[0] = startvno and indices[v_in_path-1] = endvno */
    for (i = 0 ; i < v_in_path ; i++)
    {
      vno = indices[i] ;
      if (vno == 0 || vno == Gdiag_no)
        DiagBreak() ;
      mris->vertices[vno].marked = 1 ;  /* mark it as part of path now */
      bnew->vertices[i] = vno ;
    }
    bnew->num_vertices = v_in_path ;
    
    sse = fbnd_sse(bnew, mris, target_curv, l_curv, l_len) ;
    total_length = fbnd_length(bnew, mris) ;
    if (niter++ > 20 && last_sse < sse)
      break ;
    fbnd_set_marks(bnew, mris, 1) ;
    fbnd_set_marks(bold, mris, 0) ;
    fbnd_copy(bnew, bold) ;


    /*    if (sse < last_sse)*/
      fbnd_copy(bnew, bold) ;

    printf("sse = %2.4f (%2.4f, %2.3f%%), %d vertices in path\n", 
           sse, last_sse, 100*(last_sse-sse)/last_sse,bnew->num_vertices) ;
    if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
      redraw() ;
  } while (!FZERO(sse-last_sse)) ;
  free(indices) ;

}
static double
fbnd_length(FBND_BOUNDARY *b, MRI_SURFACE *mris)
{
  double total_length, length ;
  int    i, vno ;
  VERTEX *v, *v_last = NULL ;
  
  /* compute length of current line */
  for (total_length = 0.0, i = 0 ; i < b->num_vertices ; i++)
  {
    vno = b->vertices[i] ;
    v = &mris->vertices[vno] ;
    if (i > 0)
    {
      length = sqrt(SQR(v->x-v_last->x) + 
                    SQR(v->y-v_last->y) + SQR(v->z-v_last->z)) ;
      total_length += length ;
    }
    v_last = v ;
  }  
  return(total_length) ;
}

static double
fbnd_sse(FBND_BOUNDARY *b, MRI_SURFACE *mris, double target_curv, 
         double l_curv, double l_len)
{
  double sse, curv_error, length ;
  int    i, vno ;
  VERTEX *v, *v_last = NULL ;
  
  /* compute length of current line */
  for (sse = 0.0, i = 0 ; i < b->num_vertices ; i++)
  {
    vno = b->vertices[i] ;
    v = &mris->vertices[vno] ;
    curv_error = (target_curv - v->curv) ;
    sse += l_curv * (curv_error*curv_error) ;
    if (i > 0)
    {
      length = sqrt(SQR(v->x-v_last->x) + 
                    SQR(v->y-v_last->y) + SQR(v->z-v_last->z)) ;
      sse += l_len * length ;
    }
    v_last = v ;
  }  
  return(sse) ;
}

static int
find_best_path(MRI_SURFACE *mris, int start_vno, int end_vno,
               int *path_indices, double target_curv, double l_curv, double l_len)
{
  int    *nbrs, *new_nbrs, num_new_nbrs, num_nbrs, n, done = 0, i, vno,
          min_n ;
  double sse, min_d, d ;
  VERTEX *v, *vn ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
    mris->vertices[vno].d = 0.0 ;

  nbrs = (int *)calloc(mris->nvertices, sizeof(int)) ;
  new_nbrs = (int *)calloc(mris->nvertices, sizeof(int)) ;
  v = &mris->vertices[end_vno] ;
  done = FALSE ;
  for (num_nbrs = n = 0 ; n < v->vnum ; n++)
  {
    vno = v->v[n] ;
    vn = &mris->vertices[vno] ;
    if (vn->marked && vno != start_vno)
      continue ;
    vn->d = 
      l_len * sqrt(SQR(v->x-vn->x) + SQR(v->y-vn->y) + SQR(v->z-vn->z)) +
      l_curv * ((vn->curv-target_curv)*(vn->curv-target_curv));
    if (vno == start_vno)
      done = TRUE ;
    nbrs[num_nbrs++] = vno ;
  }

  do  /* keep expanding front until start_vno is found */
  {
    for (num_new_nbrs = i = 0 ; i < num_nbrs ; i++)
    {
      v = &mris->vertices[nbrs[i]] ;
      for (n = 0 ; n < v->vnum ; n++)
      {
        vno = v->v[n] ;
        vn = &mris->vertices[vno] ;
        if (vn->marked && (vno != start_vno))  /* already visited */
          continue ;
        d = v->d +
          l_len * sqrt(SQR(v->x-vn->x) + SQR(v->y-vn->y) + SQR(v->z-vn->z)) +
          l_curv * ((vn->curv-target_curv)*(vn->curv-target_curv));
        if (!FZERO(vn->d))  /* already added */
        {
          if (d < vn->d)
            vn->d = d ; 
          continue ;    /* don't add it to nbr list twice */
        }
        if (vno == start_vno)
        {
          done = TRUE ;
          break ;
        }
        vn->d = d ;
        new_nbrs[num_new_nbrs++] = vno ;
      }
    }

    /* copy new nbr list into old one and do it again */
    if (done == FALSE)
      for (num_nbrs = 0 ; num_nbrs < num_new_nbrs ; num_nbrs++)
        nbrs[num_nbrs] = new_nbrs[num_nbrs] ;
  } while (done == FALSE) ;

  /* now search from start_vno to find shortest total path */
  done = FALSE ; sse = 0.0 ;
  path_indices[0] = vno = start_vno ; num_nbrs = 1 ;
  v = &mris->vertices[vno] ;
  do
  {
    min_n = -1 ; min_d = 10000000 ;
    for (n = 0 ; n < v->vnum ; n++)
    {
      vno = v->v[n] ;
      vn = &mris->vertices[vno] ;
      if (vno == end_vno)   /* special case - only examine end vertex with d==0 */
      {
        done = TRUE ;
        min_n = n ; min_d = vn->d ;
        break ;
      }
      if (FZERO(vn->d))   /* wasn't visited */
        continue ;
      if (vn->d < min_d || min_n < 0)
      {
        min_d = vn->d ;
        min_n = n ;
      }
    }
    vno = v->v[min_n] ;
    path_indices[num_nbrs++] = vno ;
    v = &mris->vertices[vno] ;
    sse += v->d ;
  } while (done == FALSE) ;
  return(num_nbrs) ;
}


void
plot_curv(int closedcurveflag)
{
  int         i,j,k,m;
  float       dx0,dy0,dz0,dx1,dy1,dz1,nx,ny,nz,x1,y1,z1,x2,y2,z2;
  float       f1,f2,d,d0,d1,a,x,y,z,xi,yi,zi,xj,yj,zj,s1,s2, x0, y0, z0,
              dx, dy, dz, dist ;
  VERTEX *v1,*v2,*vi,*vj;
  FILE        *fp ;


#define LINE_FNAME "surfer_curv.dat"

  fprintf(stderr, "generating data file %s with entries:\n", LINE_FNAME) ;
  fprintf(stderr, "vno x y z distance curv val val2 stat amp "
          "deg normalized_deg radians\n") ;
  if (nmarked<2) {
    printf("surfer: needs at least 2 marked vertices\n");PR return;}
  fp = fopen(LINE_FNAME, "w") ;
#if 0
  for (k=0;k<mris->nvertices;k++)
    mris->vertices[k].oripflag = mris->vertices[k].ripflag;
#endif
  if (closedcurveflag)
  {
    marked[nmarked] = marked[0];
    nmarked++;
  }
  x0 = mris->vertices[marked[0]].x ;
  y0 = mris->vertices[marked[0]].y ;
  z0 = mris->vertices[marked[0]].z ;
  for (i=0;i<nmarked-1;i++)
  {
    printf("surfer: i=%d\n",i);
    j = i+1;
    vi = &mris->vertices[marked[i]];
    vj = &mris->vertices[marked[j]];
    xi = vi->x;
    yi = vi->y;
    zi = vi->z;
    xj = vj->x;
    yj = vj->y;
    zj = vj->z;
    dx0 = vj->x-vi->x;
    dy0 = vj->y-vi->y;
    dz0 = vj->z-vi->z;
    dx1 = vi->nx+vj->nx;
    dy1 = vi->ny+vj->ny;
    dz1 = vi->nz+vj->nz;
    nx = -dy1*dz0 + dy0*dz1;
    ny = dx1*dz0 - dx0*dz1;
    nz = -dx1*dy0 + dx0*dy1;
    d = sqrt(nx*nx+ny*ny+nz*nz);
    nx /= d; ny /= d; nz /= d;
    d0 = sqrt(dx0*dx0+dy0*dy0+dz0*dz0);
    dx0 /= d0; dy0 /= d0; dz0 /= d0;
    d1 = sqrt(dx1*dx1+dy1*dy1+dz1*dz1);
    dx1 /= d1; dy1 /= d1; dz1 /= d1;
    for (k=0;k<mris->nvertices;k++)
    if (!mris->vertices[k].ripflag) 
    {
      v1 = &mris->vertices[k];
      x1 = v1->x - xi;
      y1 = v1->y - yi;
      z1 = v1->z - zi;
      s1 = nx*x1+ny*y1+nz*z1;
      for (m=0;m<v1->vnum;m++)
      if (!mris->vertices[v1->v[m]].ripflag)
      {
        v2 = &mris->vertices[v1->v[m]];
        x2 = v2->x - xi;   /* vector from point to a nbr */
        y2 = v2->y - yi;
        z2 = v2->z - zi;
        s2 = nx*x2+ny*y2+nz*z2;  
        if (s1*s2<=0)
        {
          a = s2/(s2-s1);
          x = a*x1+(1-a)*x2;
          y = a*y1+(1-a)*y2;
          z = a*z1+(1-a)*z2;
          f1 = x*dx0+y*dy0+z*dz0;
          f2 = x*dx1+y*dy1+z*dz1;
          if (f1>=-1&&f1<=d0+1&&f2>=-5.0&&f2<=5.0 && v1->marked < 2)
          {
            
            if (k == 26708)
              DiagBreak() ;

            v1->marked = 2 ;
            dx = v1->x - x0 ; dy = v1->y - y0 ; dz = v1->z - z0 ; 
            dist = sqrt(dx*dx + dy*dy + dz*dz) ;
#if 0
            printf("surfer: delete connection (%d to %d)\n",k,v1->v[m]);
            printf("surfer: {%f,%f,%f} - {%f,%f,%f}\nf1=%f, f2=%f\n",
                   x1,y1,z1,x2,y2,z2,f1,f2);
            v1->ripflag = TRUE;
#else
            fprintf(fp, "%d %f %f %f %f %f %f %f %f %f %f %f %f\n",
                    k, v1->x, v1->y, v1->z,
                    dist, v1->curv, v1->val, v1->val2, v1->stat,
                    hypot(v1->val,v1->val2),
                    (float)(atan2(v1->val2,v1->val)*180/M_PI),
                    (float)(atan2(v1->val2,v1->val)/(2*M_PI)), 
                    (float)(atan2(v1->val2,v1->val))) ;
                    
#endif            
          }
        }
      }
    }
  }
  PR
  clear_vertex_marks();
  fclose(fp) ;
}

void
clear_vertex_marks(void)
{
  int i;

  for (i=0;i<nmarked;i++)
    mris->vertices[marked[i]].marked = FALSE;
  nmarked = 0;
  if (area)
  {
    LabelUnmark(area, mris) ;
    LabelFree(&area) ;
  }
}
void
clear_all_vertex_marks(void)
{
  int i;

  for (i=0;i<mris->nvertices;i++)
    mris->vertices[i].marked = FALSE;
  nmarked = 0;
  if (area)
    {
    LabelUnmark(area, mris) ;
    LabelFree(&area) ;
  }
}

void
mark_vertex(int vindex, int onoroff)
{
  int i,j;
  VERTEX *v ;

  mris->vertices[vindex].marked = onoroff;
  for (i=0;i<nmarked&&marked[i]!=vindex;i++);
  v = &mris->vertices[vindex] ;
  if ((onoroff==FALSE)&&(i<nmarked))
  {
    v->marked = 0 ;
    nmarked--;
    for (j=i;j<nmarked;j++) marked[j] = marked[j+1];
    printf("surfer: vertex %d unmarked\n",vindex);
  }
  if ((onoroff==TRUE)&&(i==nmarked))
  {
    if (nmarked==MAXMARKED-1)
      printf("surfer: too many marked vertices\n");
    else
    {
      marked[nmarked] = vindex;
      printf("surfer: vertex %d marked (curv=%f, stat=%f)\n",
             vindex,mris->vertices[vindex].curv,mris->vertices[vindex].stat);
      printf("x = (%2.1f, %2.1f, %2.1f), n = (%2.1f, %2.1f, %2.1f).\n",
             v->x, v->y, v->z, v->nx, v->ny, v->nz) ;
      nmarked++;
    }
  }
  else if (onoroff > 1)
  {
    v = &mris->vertices[vindex] ;
    v->marked = onoroff ;
  }
  PR
}

void 
mark_annotation(int vno_annot)
{
  int    vno, annotation ;
  VERTEX *v ;

  if (vno_annot < 0)
  {
    fprintf(stderr, "no vertex currently selected...\n") ;
    return ;
  }

  v = &mris->vertices[vno_annot] ; annotation = v->annotation ;
  fprintf(stderr, "marking annotation %d...\n", annotation) ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (v->annotation == annotation)
      v->marked = 1  ;
  }
}
void
mark_faces(int vno)
{
  int    n ;
  VERTEX *v;

  if (vno < 0 || vno >= mris->nvertices)
    return ;
  v = &mris->vertices[vno] ;
  for (n = 0 ; n < v->num ; n++)
    mark_face(v->f[n]) ;
}
void
mark_face(int fno)
{
  int    i ;
  FACE   *f;
  VERTEX *v;

  /* offscreen render opengl bug: RGBcolor(white) => surrounding faces color */

  if ((fno >= mris->nfaces)||(fno<0)) 
  {
    if (fno != -1)
      printf ("surfer: ### face index %d out of bounds\n",fno);
    return;
  }

#if 0
  if (onoroff==FALSE)
    set_color(0.0,0.0,GREEN_RED_CURV);
  else
#endif
  {
    if (blackcursorflag)  RGBcolor(0,0,0);
    else                  RGBcolor(0,255,255);
  }

  f = &mris->faces[fno] ;
  RGBcolor(0,255,255);
  bgnpolygon();
  for (i = 0 ; i< VERTICES_PER_FACE ; i++)
  {
    v = &mris->vertices[f->v[i]];
    load_brain_coords(v->nx,v->ny,v->nz,v1);
    n3f(v1);
    load_brain_coords(v->x+mup*f->nx,v->y+mup*f->ny,v->z+mup*f->nz,v1);
    v3f(v1);
  }
#if 0
  linewidth(CURSOR_LINE_PIX_WIDTH);
  load_brain_coords(f->nx,f->ny,f->nz,v1);
  n3f(v1);
#endif
  endpolygon();

#if 0
  if (bigcursorflag) 
  {
    for (k=0;k<vselect->num;k++) 
    {
      bgnquadrangle();
      f = &mris->faces[vselect->f[k]];
      for (n=0;n<VERTICES_PER_FACE;n++) 
      {
        v = &mris->vertices[f->v[n]];
        load_brain_coords(v->nx,v->ny,v->nz,v1);
        n3f(v1);
        load_brain_coords(v->x+cup*v->nx,v->y+cup*v->ny,v->z+cup*v->nz,v1);
        v3f(v1);
      }
      endpolygon();
    }
  }
#endif
}
void
prompt_for_parameters(void)
{
  float  f1, f2, f3, f4, f5, f6, f7 ;
  printf("surfer: enter wt,wa,ws,wn,wc,wsh,wbn ");
  printf("surfer: (%5.2f,%5.2f,%5.2f,%5.2f,%5.2f,%5.2f,%5.2f)\n",
         wt,wa,ws,wn,wc,wsh,wbn);
  scanf("%f %f %f %f %f %f %f",&f1,&f2,&f3,&f4,&f5,&f6,&f7);
  wt = f1 ; wa = f2 ; ws = f3 ; wn = f4 ; wc = f5 ; wsh = f6 ; wbn = f7 ;
  PR
}

void
prompt_for_drawmask(void)
{
  printf("surfer: enter mask (curv,area,shear,shearvec,normvec,movevec) = %d: ",
         drawmask);
  scanf("%d",&drawmask);
  if ((drawmask&1)!=0)   surfcolor = CURVATURE_OR_SULCUS;
  if ((drawmask&2)!=0)   surfcolor = AREAL_DISTORTION;
  if ((drawmask&4)!=0)   surfcolor = SHEAR_DISTORTION;
  if ((drawmask&8)!=0)   shearvecflag = TRUE;
  if ((drawmask&16)!=0)  normvecflag = TRUE;
  if ((drawmask&32)!=0)  movevecflag = TRUE;
  PR
} 

void
fill_triangle(float x0,float y0,float z0,float x1,float y1,float z1,
              float x2,float y2,float z2)
{
  int i,j,imnr;
  float d0,d1,d2,dmax,u,v;
  float px,py,pz,px0,py0,pz0,px1,py1,pz1;
  int numu,numv;

  d0 = sqrt(SQR(x1-x0)+SQR(y1-y0)+SQR(z1-z0));
  d1 = sqrt(SQR(x2-x1)+SQR(y2-y1)+SQR(z2-z1));
  d2 = sqrt(SQR(x0-x2)+SQR(y0-y2)+SQR(z0-z2));
  dmax = (d0>=d1&&d0>=d2)?d0:(d1>=d0&&d1>=d2)?d1:d2;
  numu = ceil(2*d0);
  numv = ceil(2*dmax);
  if (numu==0) numu=1;
  if (numv==0) numv=1;
  for (v=0;v<=numv;v++)
  {
    px0 = x0 + (x2-x0)*v/numv;
    py0 = y0 + (y2-y0)*v/numv;
    pz0 = z0 + (z2-z0)*v/numv;
    px1 = x1 + (x2-x1)*v/numv;
    py1 = y1 + (y2-y1)*v/numv;
    pz1 = z1 + (z2-z1)*v/numv;
    for (u=0;u<=numu;u++)
    {
      px = px0 + (px1-px0)*u/numu;
      py = py0 + (py1-py0)*u/numu;
      pz = pz0 + (pz1-pz0)*u/numu;
      if (curvim_allocated) {
        imnr = (int)((py-mris2->yctr)/fillscale+numimg/2.0+0.5);
        i = (int)(IMGSIZE/2.0-(pz-mris2->zctr)/fillscale+0.5);
        j = (int)(IMGSIZE/2.0-(px-mris2->xctr)/fillscale+0.5);
        curvim[imnr][i][j] = 255;
      }
      else {
        imnr = (int)((py-mris->yctr)/fillscale+numimg/2.0+0.5);
        i = (int)(IMGSIZE/2.0-(pz-mris->zctr)/fillscale+0.5);
        j = (int)(IMGSIZE/2.0-(px-mris->xctr)/fillscale+0.5);
        fill[imnr][i][j] = 255;
      }
    }
  }
}

void
fill_surface(void)
{
  int i,j,k;
  int totalfilled,newfilled;

  numimg = IMGSIZE;
  for (k=0;k<numimg;k++)
  {
    fill[k] = (unsigned char **)lcalloc(IMGSIZE,sizeof(char *));
    for (i=0;i<IMGSIZE;i++)
    {
      fill[k][i] = (unsigned char *)lcalloc(IMGSIZE,sizeof(char));
    }
  }

  for (k=0;k<mris->nfaces;k++)
  {
    fill_triangle(mris->vertices[mris->faces[k].v[0]].x,
                  mris->vertices[mris->faces[k].v[0]].y,
                  mris->vertices[mris->faces[k].v[0]].z,
                  mris->vertices[mris->faces[k].v[1]].x,
                  mris->vertices[mris->faces[k].v[1]].y,
                  mris->vertices[mris->faces[k].v[1]].z,
                  mris->vertices[mris->faces[k].v[2]].x,
                  mris->vertices[mris->faces[k].v[2]].y,
                  mris->vertices[mris->faces[k].v[2]].z);
#if VERTICES_PER_FACE == 4
    fill_triangle(mris->vertices[mris->faces[k].v[0]].x,
                  mris->vertices[mris->faces[k].v[0]].y,
                  mris->vertices[mris->faces[k].v[0]].z,
                  mris->vertices[mris->faces[k].v[2]].x,
                  mris->vertices[mris->faces[k].v[2]].y,
                  mris->vertices[mris->faces[k].v[2]].z,
                  mris->vertices[mris->faces[k].v[3]].x,
                  mris->vertices[mris->faces[k].v[3]].y,
                  mris->vertices[mris->faces[k].v[3]].z);
#endif
  }

  fill[1][1][1] = 64;
  totalfilled = newfilled = 1;
  while (newfilled>0)
  {
    newfilled = 0;
    for (k=1;k<numimg-1;k++)
    for (i=1;i<IMGSIZE-1;i++)
    for (j=1;j<IMGSIZE-1;j++)
    if (fill[k][i][j]==0)
    if (fill[k-1][i][j]==64||fill[k][i-1][j]==64||fill[k][i][j-1]==64)
    {
      fill[k][i][j] = 64;
      newfilled++;
    }
    for (k=numimg-2;k>=1;k--)
    for (i=IMGSIZE-2;i>=1;i--)
    for (j=IMGSIZE-2;j>=1;j--)
    if (fill[k][i][j]==0)
    if (fill[k+1][i][j]==64||fill[k][i+1][j]==64||fill[k][i][j+1]==64)
    {
      fill[k][i][j] = 64;
      newfilled++;
    }
    totalfilled += newfilled;
    printf("surfer: filled (fill): new=%d, total=%d of %d\n",
           newfilled,totalfilled,(numimg-2)*(IMGSIZE-2)*(IMGSIZE-2));
  }
  for (k=1;k<numimg-1;k++)
  for (i=1;i<IMGSIZE-1;i++)
  for (j=1;j<IMGSIZE-1;j++)
    fill[k][i][j] = (fill[k][i][j]==0)?128:0;
  PR
}

void
fill_second_surface(void)
{
  int i,j,k;
  int totalfilled,newfilled;

  if (!curvim_allocated) {
    printf("surfer: ### fill_second_surface failed: first make curv images\n");
    PR return; }

  for (k=0;k<mris2->nfaces;k++)
  {
    fill_triangle(mris2->vertices[mris2->faces[k].v[0]].x,
                  mris2->vertices[mris2->faces[k].v[0]].y,
                  mris2->vertices[mris2->faces[k].v[0]].z,
                  mris2->vertices[mris2->faces[k].v[1]].x,
                  mris2->vertices[mris2->faces[k].v[1]].y,
                  mris2->vertices[mris2->faces[k].v[1]].z,
                  mris2->vertices[mris2->faces[k].v[2]].x,
                  mris2->vertices[mris2->faces[k].v[2]].y,
                  mris2->vertices[mris2->faces[k].v[2]].z);
#if VERTICES_PER_FACE == 4
    fill_triangle(mris2->vertices[mris2->faces[k].v[0]].x,
                  mris2->vertices[mris2->faces[k].v[0]].y,
                  mris2->vertices[mris2->faces[k].v[0]].z,
                  mris2->vertices[mris2->faces[k].v[2]].x,
                  mris2->vertices[mris2->faces[k].v[2]].y,
                  mris2->vertices[mris2->faces[k].v[2]].z,
                  mris2->vertices[mris2->faces[k].v[3]].x,
                  mris2->vertices[mris2->faces[k].v[3]].y,
                  mris2->vertices[mris2->faces[k].v[3]].z);
#endif
  }

  ocurvim[1][1][1] = 64;   /* ocurvim for tmp: don't overwrite loaded curvim */
  totalfilled = newfilled = 1;
  while (newfilled>0)
  {
    newfilled = 0;
    for (k=1;k<numimg-1;k++)
    for (i=1;i<IMGSIZE-1;i++)
    for (j=1;j<IMGSIZE-1;j++)
    if (ocurvim[k][i][j]==0)
    if (ocurvim[k-1][i][j]==64||ocurvim[k][i-1][j]==64||ocurvim[k][i][j-1]==64)
    {
      ocurvim[k][i][j] = 64;
      newfilled++;
    }
    for (k=numimg-2;k>=1;k--)
    for (i=IMGSIZE-2;i>=1;i--)
    for (j=IMGSIZE-2;j>=1;j--)
    if (ocurvim[k][i][j]==0)
    if (ocurvim[k+1][i][j]==64||ocurvim[k][i+1][j]==64||ocurvim[k][i][j+1]==64)
    {
      ocurvim[k][i][j] = 64;
      newfilled++;
    }
    totalfilled += newfilled;
    printf("surfer: filled (curvim/second): new=%d, total=%d of %d\n",
           newfilled,totalfilled,(numimg-2)*(IMGSIZE-2)*(IMGSIZE-2));
  }
  for (k=1;k<numimg-1;k++)
  for (i=1;i<IMGSIZE-1;i++)
  for (j=1;j<IMGSIZE-1;j++)
    if (ocurvim[k][i][j]==0)
      curvimflags[k][i][j] |= CURVIM_FILLED;
  PR
}

void
resize_buffers(long frame_xdim, long frame_ydim)
{
  free(framebuff);
  free(framebuff2);
  free(framebuff3);
  free(binbuff);
  framebuff = (unsigned long *)calloc(frame_xdim*frame_ydim,sizeof(long));
  framebuff2 = (unsigned long *)calloc(frame_xdim*frame_ydim,sizeof(long));
  framebuff3 = (unsigned long *)calloc(frame_xdim*frame_ydim,sizeof(long));
  binbuff = (unsigned char *)calloc(3*frame_xdim*frame_ydim,sizeof(char));
}

void
open_window(char *name)
{
#ifdef OPENGL
  XSizeHints hin;

  if (openglwindowflag) {
    printf("surfer: ### GL window already open: can't open second\n");PR
    return; }
  xmin=ymin=zmin= -(xmax=ymax=zmax=fov);

  if (doublebufferflag) {
    tkoInitDisplayMode(TKO_DOUBLE | TKO_RGB | TKO_DEPTH);
    printf("surfer: double buffered window\n");
  }
  else {
    tkoInitDisplayMode(TKO_SINGLE | TKO_RGB | TKO_DEPTH);
    printf("surfer: single buffered window\n");
  }

  if (renderoffscreen) {
    renderoffscreen1 = renderoffscreen; /* val at winopen time safe from tcl */
    tkoInitPosition(MOTIF_XFUDGE+5,MOTIF_YFUDGE+5,frame_xdim,frame_ydim);
    tkoInitPixmap(frame_xdim,frame_ydim);
    printf("surfer: ### rendering to offscreen window ###\n");PR
  }
  else {
    if (!initpositiondoneflag)
      tkoInitPosition(MOTIF_XFUDGE+wx0,(1024-wy0-frame_ydim)+MOTIF_XFUDGE,
                                                       frame_xdim,frame_ydim);
    tkoInitWindow(name);
    hin.max_width = hin.max_height = 1024;             /* maxsize */
    hin.min_aspect.x = hin.max_aspect.x = frame_xdim;  /* keepaspect */
    hin.min_aspect.y = hin.max_aspect.y = frame_ydim;
    hin.flags = PMaxSize|PAspect;
    XSetWMNormalHints(xDisplay, w.wMain, &hin);
  }

  glLoadIdentity();
  glMatrixMode(GL_PROJECTION);
  glOrtho(-fov*sf, fov*sf, -fov*sf, fov*sf, -10.0*fov*sf, 10.0*fov*sf);
  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  do_lighting_model(-1.0,-1.0,-1.0,-1.0,-1.0);
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_DEPTH_TEST);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(mris->xctr,-mris->zctr,-mris->yctr); /* MRI coords (vs. viewer:translate_brain_ */


#else  /* use gl calls */
  if (openglwindowflag) {
    printf("surfer: ### gl window already open: can't open second\n");PR
    return; }
  if (renderoffscreen) {
    printf( 
      "surfer: ### render offscreen request ignored (OpenGL version only)\n");
    renderoffscreen1 = FALSE;
  }
  xmin=ymin=zmin= -(xmax=ymax=zmax=fov);

  prefposition(0,frame_xdim-1,0,frame_ydim-1);
  foreground();  /* tcl prompt */
  winopen(name);
  keepaspect(1,1);     /* resize_window assumes square */
  maxsize(1024,1024);
  winconstraints();    /* makes re-sizeable */
  drawmode(NORMALDRAW);
  RGBmode();
  if (doublebufferflag) doublebuffer();
  else                  singlebuffer();
  gconfig();
  zbuffer(TRUE);
  mmode(MVIEWING);
  ortho(-fov*sf, fov*sf, -fov*sf, fov*sf, -10.0*fov*sf, 10.0*fov*sf);
  do_lighting_model(-1.0,-1.0,-1.0,-1.0,-1.0);
  qdevice(ESCKEY);
  qdevice(REDRAW);
  qdevice(LEFTMOUSE);
  qdevice(RIGHTMOUSE);
  qdevice(MIDDLEMOUSE);
  qdevice(UPARROWKEY);
  qdevice(DOWNARROWKEY);
  qdevice(LEFTARROWKEY);
  qdevice(RIGHTARROWKEY);
  qdevice(DELKEY);
  qdevice(PAGEDOWNKEY);
  qdevice(PAD2);
  qdevice(PAD4);
  qdevice(PAD6);
  qdevice(PAD8);
  qdevice(LEFTSHIFTKEY);
  qdevice(RIGHTSHIFTKEY);
  qdevice(LEFTCTRLKEY);
  qdevice(RIGHTCTRLKEY);
  qdevice(LEFTALTKEY);
  qdevice(RIGHTALTKEY);
  qdevice(KEYBD);
  translate(mris->xctr,-mris->zctr,-mris->yctr);
#endif

  openglwindowflag = TRUE;
}

void
swap_buffers(void)  /* so tcl sees OpenGL define */
{
  swapbuffers();
}

void
to_single_buffer(void)
{
  if (!doublebufferflag) return;
#ifdef OPENGL
  /* TODO: to toggle single/double in X: init second, glXChooseVisual */
  printf("surfer: ### toggle single/double not implemented in OPENGL\n");PR
  printf("surfer: ### tcl: set doublebufferflag FALSE before open_window\n");PR
  printf("surfer:     csh: setenv doublebufferflag 0; restart tksurfer\n");PR
#else
  if (!openglwindowflag) {
    printf("surfer: ### gl window not open: use open_window\n");PR
    return;
  }
  doublebufferflag = FALSE;
  singlebuffer();
  gconfig();
#endif
}

void
to_double_buffer(void)
{
  if (doublebufferflag) return;
#ifdef OPENGL
  /* TODO: to toggle single/double in X: init second, glXChooseVisual */
  printf("surfer: ### toggle single/double not implemented in OPENGL\n");PR
  printf("surfer: ### tcl: set doublebufferflag TRUE before open_window\n");PR
  printf("surfer:     csh: setenv doublebufferflag 1; restart tksurfer\n");PR
#else
  if (!openglwindowflag) {
    printf("surfer: ### gl window not open: use open_window\n");PR
    return;
  }
  doublebufferflag = TRUE;
  doublebuffer();
  gconfig();
#endif
}

void
do_lighting_model(float lite0,float lite1,float lite2,float lite3,
                  float newoffset)
{
#ifdef OPENGL
  /* init static arrays for non-default vars */
  static GLfloat mat0_ambient[] =  { 0.0, 0.0, 0.0, 1.0 };
  static GLfloat mat0_diffuse[] =  { OFFSET, OFFSET, OFFSET, 1.0 };
  static GLfloat mat0_emission[] = { 0.0, 0.0, 0.0, 1.0 };
  static GLfloat light0_diffuse[] = { LIGHT0_BR, LIGHT0_BR, LIGHT0_BR, 1.0 };
  static GLfloat light1_diffuse[] = { LIGHT1_BR, LIGHT1_BR, LIGHT1_BR, 1.0 };
  static GLfloat light2_diffuse[] = { LIGHT2_BR, LIGHT2_BR, LIGHT2_BR, 1.0 };
  static GLfloat light3_diffuse[] = { LIGHT3_BR, LIGHT3_BR, LIGHT3_BR, 1.0 };
  static GLfloat light0_position[] = { 0.0, 0.0, 1.0, 0.0 };/* w=0:directional*/
  static GLfloat light1_position[] = { 0.0, 0.0,-1.0, 0.0 };
  static GLfloat light2_position[] = { 0.6, 0.6, 1.6, 0.0 };/* 0.6->1.6 */
  static GLfloat light3_position[] = {-1.0, 0.0, 0.0, 0.0 };
  static GLfloat lmodel_ambient[] =  { 0.0, 0.0, 0.0, 0.0 };/* cancel 0.2 amb */

  /* neg lite => no change (func args override interface light?_br update) */
  if (lite0 < 0.0)     lite0 = light0_diffuse[0];
  if (lite1 < 0.0)     lite1 = light1_diffuse[0];
  if (lite2 < 0.0)     lite2 = light2_diffuse[0];
  if (lite3 < 0.0)     lite3 = light3_diffuse[0];
  if (newoffset < 0.0) newoffset = offset;
  else                 offset = newoffset;
  light0_br = lite0;   /* update global double copies for tcl */
  light1_br = lite1;
  light2_br = lite2;
  light3_br = lite3;

  /* material: change DIFFUSE,EMISSION (purpler: EMISSIONg=0.05*newoffset) */
  /* NOTE: on top of (!) direct (set_color,etc) effects of offset on color */
  mat0_diffuse[0] = mat0_diffuse[1] = mat0_diffuse[2] = newoffset;
  mat0_emission[0] = mat0_emission[1] = mat0_emission[2] = 0.1*newoffset;

  /* lights: change DIFFUSE */
  light0_diffuse[0] = light0_diffuse[1] = light0_diffuse[2] = lite0;
  light1_diffuse[0] = light1_diffuse[1] = light1_diffuse[2] = lite1;
  light2_diffuse[0] = light2_diffuse[1] = light2_diffuse[2] = lite2;
  light3_diffuse[0] = light3_diffuse[1] = light3_diffuse[2] = lite3;

  glMatrixMode(GL_MODELVIEW);
  pushmatrix();
    glLoadIdentity();

    glMaterialfv(GL_FRONT, GL_AMBIENT, mat0_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat0_diffuse);
    glMaterialfv(GL_FRONT, GL_EMISSION, mat0_emission);

    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, light2_diffuse);
    glLightfv(GL_LIGHT3, GL_DIFFUSE, light3_diffuse);

    /* might need to move */
    glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
    glLightfv(GL_LIGHT1, GL_POSITION, light1_position);
    glLightfv(GL_LIGHT2, GL_POSITION, light2_position);
    glLightfv(GL_LIGHT3, GL_POSITION, light3_position);

    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL); /* def: follow AMBIENT_AND_DIFFUSE */
    glEnable(GL_LIGHTING);
    if (light0_br > 0.0) glEnable(GL_LIGHT0); else glDisable(GL_LIGHT0);
    if (light1_br > 0.0) glEnable(GL_LIGHT1); else glDisable(GL_LIGHT1);
    if (light2_br > 0.0) glEnable(GL_LIGHT2); else glDisable(GL_LIGHT2);
    if (light3_br > 0.0) glEnable(GL_LIGHT3); else glDisable(GL_LIGHT3);
  popmatrix();

#else  /* use gl calls */
  /* init float arrays */
  static float material0[] = {ALPHA,1.0,
                              AMBIENT,0.0,0.0,0.0,
                              DIFFUSE,OFFSET,OFFSET,OFFSET,
                              EMISSION,0.0,0.0,0.0,
                              SHININESS,0.0, 
                              LMNULL};
  static float light0[] = {AMBIENT,0.0,0.0,0.0,
                           LCOLOR,LIGHT0_BR,LIGHT0_BR,LIGHT0_BR,
                           POSITION,0.0,0.0,1.0,0.0,
                           LMNULL};
  static float light1[] = {AMBIENT,0.0,0.0,0.0,
                           LCOLOR,LIGHT1_BR,LIGHT1_BR,LIGHT1_BR,
                           POSITION,0.0,0.0,-1.0,0.0,
                           LMNULL};
  static float light2[] = {AMBIENT,0.0,0.0,0.0,
                           LCOLOR,LIGHT2_BR,LIGHT2_BR,LIGHT2_BR,
                           POSITION,0.6,0.6,0.6,0.0,
                           LMNULL};
  static float light3[] = {AMBIENT,0.0,0.0,0.0,
                           LCOLOR,LIGHT3_BR,LIGHT3_BR,LIGHT3_BR,
                           POSITION,-1.0,0.0,0.0,0.0,
                           LMNULL};

  if (lite0 < 0.0)     lite0 = light0[5];
  if (lite1 < 0.0)     lite1 = light1[5];
  if (lite2 < 0.0)     lite2 = light2[5];
  if (lite3 < 0.0)     lite3 = light3[5];
  if (newoffset < 0.0) newoffset = offset;
  else                 offset = newoffset;

  /* material: change DIFFUSE,EMISSION (purpler:material0[12]=0.05*newoffset)*/
  material0[7] = material0[8] = material0[9] = newoffset;
  material0[11] = material0[12] = material0[13] = 0.1*newoffset;
  /* material0[12] = 0.05*newoffset; *//* purpler */

  /* lights: change LCOLOR */
  light0[5] = light0[6] = light0[7] = lite0;
  light1[5] = light1[6] = light1[7] = lite1;
  light2[5] = light2[6] = light2[7] = lite2; 
  light3[5] = light3[6] = light3[7] = lite3;

  lmdef(DEFMATERIAL,1,17,material0);  /* was: 100,17 */
  lmdef(DEFLIGHT,1,14,light0);
  lmdef(DEFLIGHT,2,14,light1);
  lmdef(DEFLIGHT,3,14,light2);
  lmdef(DEFLIGHT,4,14,light3);
  lmdef(DEFLMODEL,1,0,NULL);  /* default? */

  pushmatrix();
    loadmatrix(idmat);
    lmbind(LIGHT0,1);
    lmbind(LIGHT1,2);
    lmbind(LIGHT2,3);
    lmbind(LIGHT3,4);
    lmbind(LMODEL,1);
    lmbind(MATERIAL,1);  /* was: 100 */
  popmatrix();

  lmcolor(LMC_DIFFUSE);
#endif

  if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
  {
    printf("surfer: offset=%3.2f\n",newoffset);
    printf("surfer: light0=%3.2f, light1=%3.2f, light2=%3.2f, light3=%3.2f\n",
           lite0,lite1,lite2,lite3);
  }
}

void
invert_surface(void)
{
  int     vno ;
  VERTEX  *v ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    v->nx *= -1.0 ; v->ny *= -1.0 ; v->nz *= -1.0 ;
  }
}

void
make_filenames(char *lsubjectsdir,char *lsrname,char *lpname,char *lstem,
               char *lext)
{
  /* malloc for tcl */
  sphere_reg = (char *)malloc(NAME_LENGTH*sizeof(char));
  subjectsdir = (char *)malloc(NAME_LENGTH*sizeof(char));
  srname = (char *)malloc(NAME_LENGTH*sizeof(char));
  pname = (char *)malloc(NAME_LENGTH*sizeof(char));
  stem = (char *)malloc(NAME_LENGTH*sizeof(char));
  ext = (char *)malloc(NAME_LENGTH*sizeof(char));
  fpref = (char *)malloc(NAME_LENGTH*sizeof(char));
  ifname = (char *)malloc(NAME_LENGTH*sizeof(char));
  if2name = (char *)malloc(NAME_LENGTH*sizeof(char));
  ofname = (char *)malloc(NAME_LENGTH*sizeof(char));
  cfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  cf2name = (char *)malloc(NAME_LENGTH*sizeof(char));
  sfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  dfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  kfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  mfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  vfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  fsfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  fmfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  afname = (char *)malloc(NAME_LENGTH*sizeof(char));
  pfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  tfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  gfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  sgfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  agfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  fifname = (char *)malloc(NAME_LENGTH*sizeof(char));
  cif2name = (char *)malloc(NAME_LENGTH*sizeof(char));
  orfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  paint_fname = (char *)malloc(NAME_LENGTH*sizeof(char));
  elfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  lfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  vrfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  xffname = (char *)malloc(NAME_LENGTH*sizeof(char));
  /* following not set below */
  nfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  rfname = (char *)malloc(NAME_LENGTH*sizeof(char));
  pname2 = (char *)malloc(NAME_LENGTH*sizeof(char));
  stem2 = (char *)malloc(NAME_LENGTH*sizeof(char));
  ext2 = (char *)malloc(NAME_LENGTH*sizeof(char));
  tf2name = (char *)malloc(NAME_LENGTH*sizeof(char));

  /* make default names */
  strcpy(sphere_reg, "sphere.reg") ;
  strcpy(subjectsdir,lsubjectsdir);
  strcpy(srname,lsrname);
  strcpy(pname,lpname);
  strcpy(stem,lstem);
  strcpy(ext,lext);
  sprintf(fpref,"%s/%s/%s/%s",subjectsdir,pname,BRAINSURF_DIR,stem);
  sprintf(ifname,"%s.%s",fpref,ext);
  sprintf(if2name,"%s.%s",fpref,ext);
  sprintf(ofname,"%s.surftmp",fpref);  /* [return to] default */
  sprintf(cfname,"%s.curv",fpref);
  sprintf(cf2name,"%s.curv",fpref);
  sprintf(sfname,"%s/%s/%s-%d.dec",srname,BEM_DIR,stem,(int)dip_spacing);
  sprintf(dfname,"%s/%s/%s.dip",srname,BEM_DIR,stem);
  sprintf(kfname,"%s.sulc",fpref);
  sprintf(mfname,"%s/%s/%s/COR-",subjectsdir,pname,DEVOLT1_DIR);
  sprintf(vfname,"%s.val",fpref);
  sprintf(vfname,"%s/3/%s.val",srname,stem);
  sprintf(fsfname,"%s/%s/%s.fs",srname,FS_DIR,stem);
  sprintf(fmfname,"%s/%s/%s.fm",srname,FS_DIR,stem);
  sprintf(afname,"%s.area",fpref);
  sprintf(pfname,"%s.patch",fpref);
  sprintf(tfname,"%s/%s/%s",subjectsdir,pname,TMP_DIR);
  sprintf(gfname,"%s/%s/%s",subjectsdir,pname,IMAGE_DIR);
  sprintf(sgfname,"%s/%s",srname,IMAGE_DIR);
  sprintf(agfname,"%s/%s/%s",srname,IMAGE_DIR,"tksurfer.rgb");
sprintf(vrfname,"%s/%s/%s/%s.%s.wrl",subjectsdir,pname,BRAINSURF_DIR,stem,ext);
sprintf(xffname,"%s/%s/%s/%s",subjectsdir,pname,TRANSFORM_DIR,TALAIRACH_FNAME);
/* ~/morph/curv.rh.1000a2adj: curv (or fill), need hemi */
sprintf(fifname,"%s/%s/%s.%s.%s/COR-",subjectsdir,pname,FILLDIR_STEM,stem,ext);
sprintf(cif2name,"%s/%s/%s.%s.%s/COR-",subjectsdir,pname,CURVDIR_STEM,stem,ext);

 if (getenv("USE_WHITE") == NULL) 
   sprintf(orfname,"%s.%s",fpref, orig_suffix);
 else
   sprintf(orfname,"%s.white",fpref);
  sprintf(paint_fname,"%s.smoothwm",fpref);
  sprintf(elfname,"%s.1000a2ell",fpref);
#if 1
  sprintf(lfname,"%s/%s/%s/%s-area.label",subjectsdir,pname,LABEL_DIR,stem);
#else
  sprintf(lfname,"%s-area.label",stem);
#endif
}

void
print_help_surfer(void)
{
  printf("\n");
  printf("  [interactive only]                   mouse\n");
  printf("  -----------------                 -------------\n");
  printf("  [#mark_vertex]                       left \n");
  printf("  [#unmark_vertex]                     middle\n");
  printf("  [#clear_vertex_marks]                right\n");
  printf("  [scale up brain around click]      ctrl-left \n");
  printf("  [scale down brain around click]    ctrl-right \n");
  printf("\n");
  printf("  [interactive only]                  keyboard\n");
  printf("  -----------------                 -------------\n");
  printf("  [#print_help]                          h,?\n");
  printf("  [#find_orig_vertex_coordinates]         f\n");
  printf("  [#cut_line]                             j\n");
  printf("  [#cut_plane]                            J\n");
  printf("  [#flatten]                              L\n");
  printf("  [#cut_marked_vertices(closedcurve)]     p\n");
  printf("  [#cut_marked_vertices(opencurve)]       P\n");
  printf("  [#enter second surface]                 E\n");
  printf("  [#floodfill_marked_patch]               i\n");
  printf("  [#fill_surface,write_images]            I\n");
  printf("\n");
  printf("  commands (old surfer script)      keyboard equiv\n");
  printf("  ---------------                   --------------\n");
  printf("  redraw                                  r\n");
  printf("  shrink <steps>            s[=1],S[=10],d[=100],D[=1000]\n");
  printf("  normalize_area                          U\n");
  printf("  smooth_curv <steps>                     l[=5]\n");
  printf("  smooth_val_sparse <steps>               m[=5]\n");
  printf("  smooth_curvim_sparse <steps>          [script]\n");
  printf("  smooth_fs <steps>                     [script]\n");
  printf("  invert_vertex <vno>                   [script]\n");
  printf("  invert_face <fno>                     [script]\n");
  printf("  mark_annotation                       [script]\n");
  printf("  mark_faces <vno>                      [script]\n");
  printf("  mark_face <fno>                       [script]\n");
  printf("  dump_vertex <vno>                     [script]\n");
  printf("  show_flat_regions surf thresh         [script]\n");
  printf("  val_to_mark                           [script]\n");
  printf("  val_to_curv                           [script]\n");
  printf("  val_to_stat                           [script]\n");
  printf("  stat_to_val                           [script]\n");

  printf("  read_imag_vals <fname>                [script]\n");
  printf("  read_soltimecourse <fname>            [script]\n");
  printf("  sol_plot <timept> <plot type>         [script]\n");

  printf("  remove_triangle_links            [script]\n");

  printf("  label_to_stat                     [script]\n");
  printf("  f_to_t                           [script]\n");
  printf("  t_to_p                           [script]\n");
  printf("  f_to_p                           [script]\n");

  printf("  curv_to_val                           [script]\n");
  printf("  read_parcellation                     <parc. name> <lut name>\n");
  printf("  read_and_smooth_parcellation          <parc. name> <lut name>"
         " <siter> <miter>\n");
  printf("  read_curv_to_val                      [script]\n");
  printf("  read_disc <subject name>              [script]\n");
  printf("  deconvolve_weights <weight file name> <scale file name>\n");
  printf("  read_stds <cond no>                   [script]\n");
  printf("  mask_label                            [script]\n");
  printf("  dump_faces <vno>                      [script]\n");
  printf("  read_binary_curv                        R\n");
  printf("  read_binary_sulc                        K\n");
  printf("  read_binary_values                      q\n");
  printf("  read_binary_patch                       b\n");
  printf("  write_binary_curv                       W\n");
  printf("  write_binary_values                     Q\n");
  printf("  write_binary_areas                      G\n");
  printf("  write_binary_surface                    w\n");
  printf("  write_binary_patch                      B\n");
  printf("  write_dipoles                           N\n");
  printf("  write_decimation                        N\n");
  printf("  subsample_orient <orientthresh>      n [0.03]\n");
  printf("  compute_curvature                       c\n");
  printf("  clear_curvature                         C\n");
  printf("  draw_radius                             X\n");
  printf("  draw_theta                              Y\n");
  printf("  save_rgb                                T\n");
  printf("  rotate_brain_x <deg>             right/leftarr[=18]\n");
  printf("  rotate_brain_y <deg>               up/downarr[=18]\n");
  printf("  rotate_brain_z <deg>               pgdown/del[=18]\n");
  printf("  translate_brain_x <mm>           right/leftpad[=10]\n");
  printf("  translate_brain_y <mm>            up/downpad[=10]\n");
  printf("  scaledown_lg,scaleup_lg                { }\n");
  printf("  scaledown_sm,scaleup_sm                [ ]\n");
  printf("  restore_zero_position                   0\n");
  printf("  restore_initial_position                1\n");
  printf("  make_lateral_view                       2\n");
  printf("  raise_mri_force                      + [*=1.1]\n");
  printf("  lower_mri_force                      - [/=1.1]\n");
  printf("  exit                                   esc\n");
  printf("\n");
  printf("  setparms (keyboard only now)      keyboard equiv\n");
  printf("  ---------------                   --------------\n");
  printf("  set fslope <float>               / [/=1.5], * [*=1.5]\n");
  printf("  set fmid <float>                 - [-=1],   + [+=1]\n");
  printf("  set foffset <float>              - [-=1],   + [+=1]\n");
  printf("  set cslope <float>               / [/=1.5], * [*=1.5]\n");
  printf("  set cmid <float>                 - [-=1],   + [+=1]\n");
  printf("  set stressthresh <float>         / [/=1.1], * [*=1.1]\n");
  printf("  set wt <float>                          u\n");
  printf("  set wa <float>                          u\n");
  printf("  set ws <float>                          u\n");
  printf("  set wn <float>                          u\n");
  printf("  set wsh <float>                         u\n");
  printf("  set wbn <float>                         u\n");
  printf("  set cthk <float mm>              ( [-=0.1], ) [+=0.1]\n");
  printf("  set expandflag <boolean>           e [toggle]\n");
  printf("  set MRIflag <boolean>              M [toggle]\n");
  printf("  set avgflag <boolean>              k [toggle]\n");
  printf("  set areaflag <boolean>             A [toggle]\n");
  printf("  set overlayflag <boolean>          o [toggle]\n");
  printf("  set ncthreshflag <boolean>         a [toggle]\n");
  printf("  set verticesflag <boolean>         v [toggle]\n");
  printf("  set surfcolor <0no,1cv/su,2ar,3sh>  t,V [toggle]\n");
  printf("  set shearvecflag <boolean>            t\n");
  printf("  set normvecflag <boolean>             t\n");
  printf("  set movevecflag <boolean>             t\n");
  printf("\n");
}

void
print_help_tksurfer(void)
{
  printf("\n");
  printf("  [in surfer window]                      mouse\n");
  printf("  -----------------                   -------------\n");
  printf("  [mark_vertex]                          left \n");
  printf("  [unmark_vertex]                       middle\n");
  printf("  [clear_vertex_marks]                   right\n");
  printf("  [scale up brain around click]         ctrl-left \n");
  printf("  [scale down brain around click]       ctrl-right \n");
  printf("\n");
  printf("  [in surfer window]                     keyboard\n");
  printf("  -----------------                    -------------\n");
  printf("  [find_orig_vertex_coordinates]          Alt-f\n");
  printf("  [cut_line(opencurve]                    Alt-j\n");
  printf("  [cut_plane]                             Alt-J\n");
  printf("  [flatten]                               Alt-L\n");
  printf("  [cut vertex]                            Alt-p\n");
  printf("  [cut_line(closedcurve)]                 Alt-P\n");
  printf("  [floodfill_marked_patch]                Alt-i\n");
  printf("\n");
  printf("  script commands [at tcl prompt]     keyboard equiv\n");
  printf("  -------------------------------     --------------\n");
  printf("  help                                    Alt-h\n");
  printf("  swapbuffers                            [script]\n");
  printf("  to_single_buffer    [GL only]          [script]\n");
  printf("  to_double_buffer    [GL only]          [script]\n");
  printf("  setsize_window     [only before open]  [script]\n");
  printf("  open_window        [only one window]   [script]\n");
  printf("  redraw           [no auto update]       Alt-r\n");
  printf("  redraw_second    [doublebuffer only]   [script]\n");
  printf("  shrink <steps>              s[=1],S[=10],d[=100],D[=1000]\n");
  printf("  area_shrink <steps>                    [script]\n");
  printf("  sphere_shrink <steps> <mm rad>         [script]\n");
  printf("  ellipsoid_project <xrad> <yrad> <zrad>  [script]\n");
  printf("  ellipsoid_morph <steps> <xrad> <yrad> <zrad>  [script]\n");
  printf("  ellipsoid_shrink <steps> <xrad> <yrad> <zrad>  [script]\n");
  printf("  normalize_binary_curvature             [script]\n");
  printf("  normalize_area                          Alt-U\n");
  printf("  smooth_curvim <steps>                  [script]\n");
  printf("  smooth_curv <steps>                     Alt-l [=5]\n");
  printf("  smooth_val <steps>                     [script]\n");
  printf("  smooth_val_sparse <steps>               Alt-m [=5]\n");
  printf("  average_curv_images <subject>          [script]\n");
  printf("  read_curv_images                       [script]\n");
  printf("  read_second_binary_curv    [targcurv]  [script]\n");
  printf("  read_second_binary_sulc                [script]\n");
  printf("  normalize_second_binary_curv           [script]\n");
  printf("  curv_to_curvim                         [script]\n");
  printf("  second_surface_curv_to_curvim          [script]\n");
  printf("  add_subject_to_average_curvim          [script]\n");
  printf("  swap_curv                              [script]\n");
  printf("  curvim_to_surface                      [script]\n");
  printf("  curvim_to_second_surface               [script]\n");
  printf("  read_binary_curv                        Alt-R\n");
  printf("  read_binary_sulc                        Alt-K\n");
  printf("  read_binary_values                     [script]\n");
  printf("  read_binary_values_frame               [script]\n");
  printf("  read_annotated_image                   [script]\n");
  printf("  read_annotations                       [script]\n");
  printf("  read_binary_patch                      [script]\n");
  printf("  read_fieldsign                         [script]\n");
  printf("  read_fsmask                            [script]\n");
  printf("  write_binary_curv                      [script]\n");
  printf("  write_binary_sulc                      [script]\n");
  printf("  write_binary_values                     Alt-Q\n");
  printf("  write_binary_areas                      Alt-G\n");
  printf("  write_binary_surface                    Alt-w\n");
  printf("  write_binary_patch                      Alt-B\n");
  printf("  write_fieldsign                        [script]\n");
  printf("  write_fsmask                           [script]\n");
  printf("  write_dipoles                          [script]\n");
  printf("  write_decimation                       [script]\n");
  printf("  write_curv_images                      [script]\n");
  printf("  write_fill_images                      [script]\n");
  printf("  fill_second_surface                    [script]\n");
  printf("  subsample_dist <num>                   [script]\n");
  printf("  subsample_orient <orientthr[0.03]>     [script]\n");
  printf("  write_subsample                        [script]\n");
  printf("  write_binary_subsample                 [script]\n");
  printf("  compute_curvature                       Alt-c\n");
  printf("  compute_CMF                            [script]\n");
  printf("  compute_cortical_thickness             [script]\n");
  printf("  clear_curvature                         Alt-C\n");
  printf("  clear_ripflags                         [script]\n");
  printf("  restore_ripflags                        Alt-u\n");
  printf("  floodfill_marked_patch <0=cut,1=fthr,2=curv>  Alt-i,I\n");
  printf("  cut_line <0=open,1=closed>            Alt-j,Alt-P\n");
  printf("  plot_curv <0=open,1=closed>            [script]\n");
  printf("  draw_fundus                            [script]\n");
  printf("  plot_marked <file name>                [script]\n");
  printf("  put_retinotopy_stats_in_vals           \n");
  printf("  cut_plane  [mark 4: 3=plane,1=save]     Alt-J\n");
  printf("  flatten                                 Alt-L\n");
  printf("  shift_values                           [script]\n");
  printf("  swap_values                            [script]\n");
  printf("  swap_stat_val                          [script]\n");
  printf("  swap_val_val2                          [script]\n");
  printf("  compute_angles                         [script]\n");
  printf("  compute_fieldsign                      [script]\n");
  printf("  draw_radius                            [script]\n");
  printf("  draw_theta                             [script]\n");
  printf("  save_rgb                                Alt-T\n");
  printf("  save_rgb_named_orig <str> [*/rgb/name] [script]\n");
  printf("  save_rgb_cmp_frame [auto-open,1st=end] [script]\n");
  printf("  open_rgb_cmp_named <str>               [script]\n");
  printf("  save_rgb_cmp_frame_named <latency>     [script]\n");
  printf("  close_rgb_cmp_named                    [script]\n");
  printf("  rotate_brain_x <deg>                right/left[=18]\n");
  printf("  rotate_brain_y <deg>                  up/down[=18]\n");
  printf("  rotate_brain_z <deg>                pageup/pagedown[=10]\n");
  printf("  translate_brain_x <mm>             padright/padleft[=10]\n");
  printf("  translate_brain_y <mm>              padup/paddown[=10]\n");
  printf("  translate_brain_z <mm>                 [script]\n");
  printf("  scale_brain <0.0-4.0x>                 [script]\n");
  printf("  scaledown_lg,scaleup_lg             Alt-{ Alt-}\n");
  printf("  scaledown_sm,scaleup_sm             Alt-[ Alt-]\n");
  printf("  restore_zero_position                   Alt-0\n");
  printf("  restore_initial_position                Alt-1\n");
  printf("  read_view_matrix                        [script]\n");
  printf("  write_view_matrix                       [script]\n");
  printf("  read_really_matrix                      [script]\n");
  printf("  write_really_matrix                     [script]\n");
  printf("  really_translate_brain <rh/lh> <ant/post> <sup/inf>   [script]\n");
  printf("  really_scale_brain <rh/lh> <ant/post> <sup/inf>       [script]\n");
  printf("  align_sphere                            [script]\n");
  printf("  really_rotate_brain_x <deg>  [N.B.: lh/rh axis]       [script]\n");
  printf("  really_rotate_brain_y <deg>  [N.B.: ant/post axis]    [script]\n");
  printf("  really_rotate_brain_z <deg>  [N.B.: sup/inf axis]     [script]\n");
  printf("  really_center_brain                     [script]\n");
  printf("  really_center_second_brain              [script]\n");
  printf("  make_lateral_view                       Alt-2\n");
  printf("  make_lateral_view_second               [script]\n");
  printf("  do_lighting_model <lit0> <lit1> <lit2> <lit3> <offset>\n");
  printf("                     %3.2f   %3.2f   %3.2f   %3.2f   %3.2f\n",
                       LIGHT0_BR,LIGHT1_BR,LIGHT2_BR,LIGHT3_BR,OFFSET);
  printf("  mriforce <up,down>                   *[=up] /]=down]\n");
  printf("  resize_window <pix>                    [script]\n");
  printf("  exit                                      esc\n");
  printf("\n");
  printf("  script setparms                     keyboard equiv\n");
  printf("  ---------------                     --------------\n");
  printf("  set fthresh <float>                    [script]\n");
  printf("  set fslope <float>  [0.0=linear]       [script]\n");
  printf("  set fcurv <float>  [0.0=linear]        [script]\n");
  printf("  set fmid <float>                       [script]\n");
  printf("  set foffset <float>                     [script]\n");
  printf("  set cslope <float>                     [script]\n");
  printf("  set cmid <float>                       [script]\n");
  printf("  set mslope <float>                     [script]\n");
  printf("  set mmid <float>                       [script]\n");
  printf("  set mstrength <float>                  [script]\n");
  printf("  set whitemid <float>                   [script]\n");
  printf("  set graymid <float>                    [script]\n");
  printf("  set stressthresh <float>               [script]\n");
  printf("  set icstrength <float>                 [script]\n");
  printf("  set angle_offset <float>               [script]\n");
  printf("  set angle_cycles <float>               [script]\n");
  printf("  set wt <float>                         [script]\n");
  printf("  set wa <float>                         [script]\n");
  printf("  set ws <float>                         [script]\n");
  printf("  set wn <float>                         [script]\n");
  printf("  set wc <float>                         [script]\n");
  printf("  set wsh <float>                        [script]\n");
  printf("  set wbn <float>                        [script]\n");
  printf("  set update <float>                     [script]\n");
  printf("  set decay <float>                      [script]\n");
  printf("  set cthk <float mm>          Alt-( [-=0.1], Alt-) [+=0.1]\n");
  printf("  set mingm <0-255>                      [script]\n");
  printf("  set blufact <float>                    [script]\n");
  printf("  set cvfact <float>                     [script]\n");
  printf("  set fadef <float>                      [script]\n");
  printf("  set mesh_linewidth <int>               [script]\n");
  printf("  set meshr <0-255>                      [script]\n");
  printf("  set meshg <0-255>                      [script]\n");
  printf("  set meshb <0-255>                      [script]\n");
  printf("  set scalebar_bright <0-255>            [script]\n");
  printf("  set scalebar_xpos <-1.0 to 1.0>        [script]\n");
  printf("  set scalebar_ypos <-1.0 to 1.0>        [script]\n");
  printf("  set colscalebar_width <-1.0 to 1.0>    [script]\n");
  printf("  set colscalebar_height <-1.0 to 1.0>   [script]\n");
  printf("  set colscalebar_xpos <-1.0 to 1.0>     [script]\n");
  printf("  set colscalebar_ypos <-1.0 to 1.0>     [script]\n");
  printf("  set ncthresh <float>                   [script]\n");
  printf("  set colscale <0wh,1he,2cr,3bgr,4two,5bw> [script]\n");
  printf("  set momentumflag <boolean>              Alt-F\n");
  printf("  set expandflag <boolean>                Alt-e\n");
  printf("  set MRIflag <boolean>                   Alt-M\n");
  printf("  set sulcflag <boolean>                 [script]\n");
  printf("  set avgflag <boolean>                  [script]\n");
  printf("  set areaflag <boolean>                  Alt-A\n");
  printf("  set complexvalflag <boolean>           [script]\n");
  printf("  set overlayflag <boolean>               Alt-o\n");
  printf("  set ncthreshflag <boolean>              Alt-a\n");
  printf("  set verticesflag <boolean>              Alt-v\n");
  printf("  set surfcolor <0no,1cv/su,2area,3shr>  [script]\n");
  printf("  set shearvecflag <boolean>             [script]\n");
  printf("  set normvecflag <boolean>              [script]\n");
  printf("  set movevecflag <boolean>              [script]\n");
  printf("  set autoscaleflag <boolean>            [script]\n");
  printf("  set revfsflag <boolean> [computetime]  [script]\n");
  printf("  set revphaseflag <boolean> [displaytime]   [script]\n");
  printf("  set truncphaseflag <boolean> [displaytime] [script]\n");
  printf("  set scalebarflag <boolean>   [1 cm]    [script]\n");
  printf("  set colscalebarflag <boolean>          [script]\n");
  printf("  set isocontourflag <boolean>           [script]\n");
  printf("  set phasecontourflag <boolean>         [script]\n");
  printf("  set doublebufferflag <boolean>         [script]\n");
  printf("  set renderoffscreen <boolean> [only works before window opened]\n");
  printf("   ==> abbrev:  tilde     star       pound     abs     rel\n");
  printf("   ==>   dirs:  <home>  <session> <defscript> <none>  <none>\n");
  printf("  setfile outsurf <[~]string>            [script]\n");
  printf("  setfile curv    <[~]string>            [script]\n");
  printf("  setfile sulc    <[~]string>            [script]\n");
  printf("  setfile patch   <[~]string>            [script]\n");
  printf("  setfile dip     <[~]string>            [script]\n");
  printf("  setfile dec     <[~]string>            [script]\n");
  printf("  setfile annot   <[~]string>            [script]\n");
  printf("  setfile label   <[~]string>            [script]\n");
  printf("  setfile area    <[~]string>            [script]\n");
  printf("  setfile script  <[~ or #]string>       [script]\n");
  printf("  setsession   <[~]/string> [this is *]  [script]\n");
  printf("  setfile val  <[* or ~]string>          [script]\n");
  printf("  setfile fs   <[* or ~]string>          [script]\n");
  printf("  setfile fm   <[* or ~]string>          [script]\n");
  printf("  setfile num_rgbdir   <[~ or *]string>  [script]\n");
  printf("  setfile named_rgbdir <[~ or *]string>  [script]\n");
  printf("  setfile rgb  <[~ or *]string>          [script]\n");
  printf("  setfile targsurf  <[~]string>          [script]\n");
  printf("  setfile targcurv  <[~]string>          [script]\n");
}

#endif /* defined(TCL) && defined(TKSURFER) */


/* end of surfer.c and start of tksurfer.c */



#if 1 /*defined(Linux) || defined(SunOS) || defined(sun)*/
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif 
#if 1 /* defined(Linux) || defined(SunOS) || defined(sun)*/
int tk_NumMainWindows = 1 ;
#endif

/* boilerplate wrap function defines for easier viewing */
#define WBEGIN (ClientData clientData,Tcl_Interp *interp,int argc,char *argv[]){
  /* rkt begin - changed to use Tcl_SetResult and TCL_VOLATILE */
#define ERR(N,S)  if(argc!=N){Tcl_SetResult(interp,S,TCL_VOLATILE);return TCL_ERROR;}
  // #define ERR(N,S)  if(argc!=N){interp->result=S;return TCL_ERROR;}
  /* rkt end */
#define WEND   return TCL_OK;}
#define REND  (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL
#define PARM (ClientData clientData,Tcl_Interp *interp,int argc,char *argv[])

/* Prototypes */
int W_swap_buffers PARM;
int W_to_single_buffer PARM;
int W_to_double_buffer PARM;
int W_open_window PARM;
int W_help  PARM; 
int W_redraw  PARM; 
int W_dump_vertex  PARM; 
int W_show_flat_regions  PARM; 
int W_val_to_mark  PARM; 
int W_val_to_curv  PARM; 
int W_val_to_stat  PARM; 
int W_stat_to_val  PARM; 

int W_read_imag_vals  PARM; 
int W_read_soltimecourse  PARM; 
int W_sol_plot  PARM; 

int W_remove_triangle_links  PARM; 

int W_label_to_stat  PARM; 
int W_f_to_t  PARM; 
int W_t_to_p  PARM; 
int W_f_to_p  PARM; 

int W_curv_to_val  PARM; 
int W_read_curv_to_val  PARM; 
int W_read_parcellation  PARM; 
int W_read_and_smooth_parcellation  PARM; 
int W_read_disc  PARM; 
int W_deconvolve_weights  PARM; 
int W_read_stds  PARM; 
int W_mark_label  PARM; 
int W_orient_sphere  PARM; 
int W_dump_faces  PARM; 
int W_mark_annotation  PARM; 
int W_mark_face  PARM; 
int W_invert_face  PARM; 
int W_mark_faces  PARM; 
int W_invert_vertex  PARM; 
int W_redraw_second  PARM; 
int W_shrink  PARM;
int W_area_shrink  PARM;
int W_sphere_shrink  PARM;
int W_ellipsoid_project  PARM;
int W_ellipsoid_morph  PARM;
int W_ellipsoid_shrink  PARM;
int W_ellipsoid_shrink_bug  PARM;
int W_curv_shrink_to_fill  PARM;
int W_smooth_curvim  PARM;
int W_smooth_curv  PARM; 
int W_smooth_val  PARM;
int W_smooth_val_sparse  PARM;
int W_smooth_curvim_sparse  PARM;
int W_smooth_fs  PARM;
int W_add_subject_to_average_curvim  PARM;
int W_read_curv_images PARM;
int W_read_second_binary_surf  PARM;
int W_read_second_binary_curv  PARM;
int W_normalize_second_binary_curv  PARM;
int W_curv_to_curvim PARM;
int W_second_surface_curv_to_curvim PARM;
int W_curvim_to_second_surface PARM;
int W_swap_curv PARM;
int W_curvim_to_surface PARM;
int W_read_binary_curv  PARM; 
int W_read_binary_sulc  PARM; 
int W_read_binary_values  PARM; 
int W_read_binary_values_frame  PARM; 
int W_read_annotated_image  PARM; 
int W_read_annotateds  PARM; 
int W_read_binary_patch  PARM; 
int W_read_fieldsign  PARM; 
int W_read_fsmask PARM; 
int W_write_binary_areas  PARM; 
int W_write_binary_surface  PARM; 
int W_write_binary_curv  PARM; 
int W_write_binary_sulc  PARM; 
int W_write_binary_values  PARM; 
int W_write_binary_patch  PARM; 
int W_write_labeled_vertices  PARM;
int W_read_labeled_vertices  PARM;
int W_read_and_color_labeled_vertices  PARM;
int W_write_fieldsign  PARM; 
int W_write_fsmask PARM; 
int W_write_vrml PARM; 
int W_write_binary_dipoles PARM; 
int W_write_binary_decimation PARM; 
int W_write_dipoles PARM; 
int W_write_decimation PARM; 
int W_write_curv_images PARM; 
int W_write_fill_images PARM; 
int W_fill_second_surface  PARM; 
int W_subsample_dist PARM; 
int W_subsample_orient PARM; 
int W_write_subsample PARM; 
int W_write_binary_subsample PARM; 
int W_compute_curvature  PARM;
int W_compute_CMF  PARM;
int W_compute_cortical_thickness PARM;
int W_clear_curvature  PARM; 
int W_clear_ripflags  PARM;
int W_restore_ripflags  PARM;
int W_floodfill_marked_patch  PARM;
int W_twocond  PARM;
int W_cut_line  PARM;
int W_plot_curv  PARM;
int W_draw_fundus  PARM;
int W_plot_marked  PARM;
int W_put_retinotopy_stats_in_vals  PARM;
int W_draw_vector  PARM;
int W_cut_plane  PARM;
int W_flatten  PARM;
int W_normalize_binary_curv  PARM; 
int W_normalize_area  PARM; 
int W_shift_values  PARM;
int W_swap_values  PARM; 
int W_swap_stat_val  PARM;
int W_swap_val_val2  PARM;
int W_compute_angles  PARM;
int W_compute_fieldsign  PARM;
int W_draw_radius PARM;
int W_draw_theta PARM;
int W_save_rgb  PARM; 
int W_save_rgb_num  PARM;
int W_save_rgb_named_orig  PARM; 
int W_save_rgb_cmp_frame  PARM; 
int W_open_rgb_cmp_named  PARM; 
int W_save_rgb_cmp_frame_named  PARM; 
int W_close_rgb_cmp_named  PARM; 
int W_rotate_brain_x  PARM; 
int W_rotate_brain_y  PARM; 
int W_rotate_brain_z  PARM; 
int W_translate_brain_x  PARM; 
int W_translate_brain_y  PARM;
int W_translate_brain_z  PARM;
int W_scale_brain  PARM;
int W_resize_window  PARM; 
int W_setsize_window  PARM; 
int W_do_lighting_model  PARM;
int W_restore_zero_position  PARM; 
int W_restore_initial_position  PARM; 
int W_make_lateral_view  PARM; 
int W_make_lateral_view_second  PARM; 
int W_read_view_matrix  PARM; 
int W_write_view_matrix  PARM; 
int W_read_really_matrix  PARM; 
int W_write_really_matrix  PARM; 
int W_really_translate_brain  PARM;
int W_really_scale_brain  PARM;
int W_align_sphere  PARM;
int W_really_rotate_brain_x  PARM;
int W_really_rotate_brain_y  PARM;
int W_really_rotate_brain_z  PARM;
int W_really_center_brain  PARM;
int W_really_center_second_brain  PARM;
int W_really_align_brain  PARM;
int W_read_binary_decimation  PARM; 
int W_read_binary_dipoles  PARM; 
int W_load_vals_from_sol  PARM; 
int W_load_var_from_sol  PARM;
int W_read_iop  PARM;
int W_read_rec  PARM;
int W_read_ncov  PARM;
int W_filter_recs  PARM;
int W_normalize_time_courses  PARM; 
int W_compute_timecourses  PARM;
int W_normalize_inverse  PARM;
int W_compute_pval_fwd  PARM;
int W_compute_select_fwd  PARM;
int W_compute_pval_inv  PARM;
int W_find_orig_vertex_coordinates  PARM; 
int W_select_orig_vertex_coordinates  PARM; 
int W_select_talairach_point  PARM; 
int W_read_orig_vertex_coordinates  PARM;

int W_save_surf  PARM;
int W_restore_surf  PARM;
int W_read_pial_vertex_coordinates  PARM;
int W_read_white_vertex_coordinates  PARM;

int W_read_canon_vertex_coordinates  PARM;
int W_send_spherical_point  PARM;
int W_send_to_subject  PARM;
int W_resend_to_subject  PARM;
int W_drawcb  PARM;
int W_read_ellipsoid_vertex_coordinates  PARM;
int W_invert_surface  PARM;
int W_fix_nonzero_vals  PARM;
int W_draw_ellipsoid_latlong  PARM;
int W_left_click PARM; 
int W_plot_all_time_courses PARM;
int W_read_plot_list  PARM;
int W_read_vertex_list  PARM;
int W_draw_cursor  PARM;
int W_mark_vertex  PARM;
int W_draw_all_cursor  PARM;
int W_draw_all_vertex_cursor  PARM;
int W_clear_all_vertex_cursor  PARM;
/* begin rkt */
int W_sclv_read_binary_values  PARM; 
int W_sclv_read_binary_values_frame  PARM; 
int W_sclv_read_bfile_values  PARM; 
int W_sclv_write_binary_values PARM;
int W_sclv_smooth  PARM; 
int W_sclv_set_current_field  PARM; 
int W_sclv_set_current_timepoint  PARM; 
int W_sclv_copy_view_settings_from_current_field PARM;
int W_sclv_copy_view_settings_from_field PARM;
int W_sclv_set_current_threshold_from_percentile PARM;
int W_sclv_send_histogram PARM;
int W_sclv_get_normalized_color_for_value PARM;
int W_clear_vertex_marks PARM;
int W_swap_vertex_fields PARM;
int W_undo_last_action PARM;
/* end rkt */

#define TkCreateMainWindow Tk_CreateMainWindow


/*=======================================================================*/
/* function wrappers and errors */
int                  W_swap_buffers  WBEGIN 
  ERR(1,"Wrong # args: swap_buffers")
                       swap_buffers();  WEND

int                  W_to_single_buffer  WBEGIN 
  ERR(1,"Wrong # args: to_single_buffer")
                       to_single_buffer();  WEND

int                  W_to_double_buffer  WBEGIN 
  ERR(1,"Wrong # args: to_double_buffer")
                       to_double_buffer();  WEND

int                  W_open_window  WBEGIN 
  ERR(1,"Wrong # args: open_window")
                       open_window(pname);  WEND

int                  W_help  WBEGIN 
  ERR(1,"Wrong # args: help")
                       print_help_tksurfer();  WEND

int                  W_redraw  WBEGIN 
  ERR(1,"Wrong # args: redraw")
                       redraw();  WEND

int                  W_redraw_second  WBEGIN 
  ERR(1,"Wrong # args: redraw_second")
                       redraw_second();  WEND

int                  W_shrink  WBEGIN
  ERR(2,"Wrong # args: shrink <steps>")
                       shrink(atoi(argv[1]));  WEND

int                  W_area_shrink  WBEGIN
  ERR(2,"Wrong # args: area_shrink <steps>")
                       area_shrink(atoi(argv[1]));  WEND

int                  W_sphere_shrink  WBEGIN
  ERR(3,"Wrong # args: sphere_shrink <steps> <mm rad>")
                       sphere_shrink(atoi(argv[1]),atof(argv[2]));  WEND

int                 W_ellipsoid_project  WBEGIN
 ERR(4,"Wrong # args: ellipsoid_project <mm:rh/lh> <ant/post> <sup/inf>")
                      ellipsoid_project(atof(argv[1]),atof(argv[2]),
                                      atof(argv[3])); WEND

int                 W_ellipsoid_morph  WBEGIN
 ERR(5,"Wrong # args: ellipsoid_morph <steps> <mm:rh/lh> <ant/post> <sup/inf>")
                      ellipsoid_morph(atoi(argv[1]),atof(argv[2]),
                                      atof(argv[3]),atof(argv[4])); WEND

int                 W_ellipsoid_shrink  WBEGIN
 ERR(5,"Wrong # args: ellipsoid_shrink <steps> <mm:rh/lh> <ant/post> <sup/inf>")
                      ellipsoid_shrink(atoi(argv[1]),atof(argv[2]), 
                                      atof(argv[3]),atof(argv[4])); WEND
int                  W_ellipsoid_shrink_bug  WBEGIN
  ERR(4,"Wrong # args: ellipsoid_shrink_bug <steps> <mm rad> <mm len>")
                       ellipsoid_shrink_bug(atoi(argv[1]),atof(argv[2]), 
                                                  atof(argv[3])); WEND
int                  W_curv_shrink_to_fill  WBEGIN
  ERR(2,"Wrong # args: curv_shrink_to_fill <steps>")
                       curv_shrink_to_fill(atoi(argv[1]));  WEND

int                  W_smooth_curvim  WBEGIN
  ERR(2,"Wrong # args: smooth_curvim <steps>")
                       smooth_curvim(atoi(argv[1]));  WEND

int                  W_smooth_curv  WBEGIN 
  ERR(2,"Wrong # args: smooth_curv <steps>")
                       smooth_curv(atoi(argv[1]));  WEND

int                  W_smooth_val  WBEGIN
  ERR(2,"Wrong # args: smooth_val <steps>")
                       smooth_val(atoi(argv[1]));  WEND

int                  W_smooth_val_sparse  WBEGIN
  ERR(2,"Wrong # args: smooth_val_sparse <steps>")
                       smooth_val_sparse(atoi(argv[1]));  WEND

int                  W_smooth_curvim_sparse  WBEGIN
  ERR(2,"Wrong # args: smooth_curvim_sparse <steps>")
                       smooth_curvim_sparse(atoi(argv[1]));  WEND

int                  W_smooth_fs  WBEGIN
  ERR(2,"Wrong # args: smooth_fs <steps>")
                       smooth_fs(atoi(argv[1]));  WEND

int                  W_add_subject_to_average_curvim  WBEGIN
  ERR(3,"Wrong # args: add_subject_to_average_curvim <subjname> <morphsubdir>")
                       add_subject_to_average_curvim(argv[1],argv[2]); WEND

int                  W_read_curv_images WBEGIN
  ERR(1,"Wrong # args: read_curv_images")
                       read_curv_images(cif2name); WEND

int                  W_read_stds WBEGIN
  ERR(2,"Wrong # args: read_stds")
                       read_stds(atoi(argv[1])); WEND

int                  W_read_second_binary_surf  WBEGIN
  ERR(1,"Wrong # args: read_second_binary_surf")
                       read_second_binary_surface(if2name); WEND

int                  W_read_second_binary_curv  WBEGIN
  ERR(1,"Wrong # args: read_second_binary_curv")
                       read_second_binary_curvature(cf2name); WEND

int                  W_normalize_second_binary_curv  WBEGIN
  ERR(1,"Wrong # args: normalize_second_binary_curv")
                       normalize_second_binary_curvature();  WEND

int                  W_curv_to_curvim WBEGIN
  ERR(1,"Wrong # args: curv_to_curvim")
                       curv_to_curvim();  WEND

int                  W_second_surface_curv_to_curvim WBEGIN
  ERR(1,"Wrong # args: second_surface_curv_to_curvim")
                       second_surface_curv_to_curvim();  WEND

int                  W_curvim_to_second_surface WBEGIN
  ERR(1,"Wrong # args: curvim_to_second_surface")
                       curvim_to_second_surface();  WEND

int                  W_swap_curv WBEGIN
  ERR(1,"Wrong # args: swap_curv")
                       swap_curv();  WEND

int                  W_curvim_to_surface WBEGIN
  ERR(1,"Wrong # args: curvim_to_surface")
                       curvim_to_surface();  WEND

int                  W_read_binary_surf  WBEGIN
  ERR(1,"Wrong # args: read_binary_surf")
                       read_binary_surface(ifname);  WEND

int                  W_read_surf  WBEGIN
  ERR(2,"Wrong # args: read_surf")
                       read_positions(argv[1]);  WEND

int                  W_save_surf  WBEGIN
  ERR(1,"Wrong # args: save_surf")
                       save_surf();  WEND

int                  W_restore_surf  WBEGIN
  ERR(1,"Wrong # args: restore_surf")
                       restore_surf();  WEND

int                  W_show_surf  WBEGIN
  ERR(2,"Wrong # args: show_surf")
                       show_surf(argv[1]);  WEND

int                  W_read_binary_curv  WBEGIN 
  ERR(1,"Wrong # args: read_binary_curv")
                       read_binary_curvature(cfname);  WEND

int                  W_read_binary_sulc  WBEGIN 
  ERR(1,"Wrong # args: read_binary_sulc")
                       read_binary_curvature(kfname);  WEND

int                  W_read_binary_values  WBEGIN 
  ERR(1,"Wrong # args: read_binary_values")
                       read_binary_values(vfname);  WEND

int                  W_read_binary_values_frame  WBEGIN 
  ERR(1,"Wrong # args: read_binary_values_frame")
                       read_binary_values_frame(vfname);  WEND

int                  W_read_annotations  WBEGIN 
  ERR(2,"Wrong # args: read_annotations")
                   read_annotations(argv[1]); WEND

int                  W_read_annotated_image  WBEGIN 
  ERR(1,"Wrong # args: read_annotated_image")
                   read_annotated_image(nfname,frame_xdim,frame_ydim); WEND

int                  W_read_binary_patch  WBEGIN 
  ERR(1,"Wrong # args: read_binary_patch")
                       read_binary_patch(pfname);  WEND

int                  W_read_fieldsign  WBEGIN 
  ERR(1,"Wrong # args: read_fieldsign")
                       read_fieldsign(fsfname);  WEND

int                  W_read_fsmask WBEGIN 
  ERR(1,"Wrong # args: read_fsmask")
                       read_fsmask(fmfname);  WEND

int                  W_write_binary_areas  WBEGIN 
  ERR(1,"Wrong # args: write_binary_areas")
                       write_binary_areas(afname);  WEND

int                  W_write_binary_surface  WBEGIN 
  ERR(1,"Wrong # args: write_binary_surface")
                       write_binary_surface(ofname);  WEND

int                  W_write_binary_curv  WBEGIN 
  ERR(1,"Wrong # args: write_binary_curv")
                       write_binary_curvature(cfname);  WEND

int                  W_write_binary_sulc  WBEGIN 
  ERR(1,"Wrong # args: write_binary_sulc")
                       write_binary_curvature(kfname);  WEND

int                  W_write_binary_values  WBEGIN 
  ERR(1,"Wrong # args: write_binary_values")
                       write_binary_values(vfname);  WEND

int                  W_write_binary_patch  WBEGIN 
  ERR(1,"Wrong # args: write_binary_patch")
                       write_binary_patch(pfname);  WEND

int                  W_write_labeled_vertices  WBEGIN
  ERR(1,"Wrong # args: write_labeled_vertices")
                       write_labeled_vertices(lfname);  WEND

int                  W_read_labeled_vertices  WBEGIN
  ERR(1,"Wrong # args: read_labeled_vertices")
                       read_labeled_vertices(lfname);  WEND

int                  W_read_and_color_labeled_vertices  WBEGIN
  ERR(4,"Wrong # args: read_and_color_labeled_vertices")
                       read_and_color_labeled_vertices(atoi(argv[1]),atoi(argv[2]), atoi(argv[3]));  WEND

int                  W_write_fieldsign  WBEGIN 
  ERR(1,"Wrong # args: write_fieldsign")
                       write_fieldsign(fsfname);  WEND

int                  W_write_fsmask WBEGIN 
  ERR(1,"Wrong # args: write_fsmask")
                       write_fsmask(fmfname);  WEND

int                  W_write_vrml WBEGIN 
  ERR(2,"Wrong # args: write_vrml <mode:0=pts,1=gray,2=curv>")
                       write_vrml(vrfname,atoi(argv[1])); WEND

int                  W_write_binary_dipoles WBEGIN 
  ERR(1,"Wrong # args: write_binary_dipoles")
                       write_binary_dipoles(dfname);  WEND

int                  W_write_binary_decimation WBEGIN 
  ERR(1,"Wrong # args: write_binary_decimation")
                       write_binary_decimation(sfname);  WEND

int                  W_write_dipoles WBEGIN 
  ERR(1,"Wrong # args: write_dipoles")
                       write_dipoles(dfname);  WEND

int                  W_write_decimation WBEGIN 
  ERR(1,"Wrong # args: write_decimation")
                       write_decimation(sfname);  WEND

int                  W_write_curv_images WBEGIN 
  ERR(1,"Wrong # args: write_curv_images")
                       write_images(curvim,cif2name);  WEND

int                  W_write_fill_images WBEGIN 
  ERR(1,"Wrong # args: write_fill_images")
                       write_images(fill,fifname);  WEND

int                  W_fill_second_surface  WBEGIN 
  ERR(1,"Wrong # args: fill_second_surface")
                       fill_second_surface();  WEND

int                  W_subsample_dist WBEGIN 
  ERR(2,"Wrong # args: subsample_dist <mm>")
                       subsample_dist(atoi(argv[1]));  WEND

int                  W_subsample_orient WBEGIN 
  ERR(2,"Wrong # args: subsample_orient <orientthresh>")
                       subsample_orient(atof(argv[1]));  WEND

int                  W_write_subsample WBEGIN 
  ERR(1,"Wrong # args: write_subsample")
                       write_subsample(sfname); WEND

int                  W_write_binary_subsample WBEGIN 
  ERR(1,"Wrong # args: write_binary_subsample")
                       write_binary_subsample(sfname); WEND

int                  W_compute_curvature  WBEGIN
  ERR(1,"Wrong # args: compute_curvature")
                       compute_curvature();  WEND

int                  W_compute_CMF  WBEGIN
  ERR(1,"Wrong # args: compute_CMF")
                       compute_CMF();  WEND

int                  W_compute_cortical_thickness WBEGIN
  ERR(1,"Wrong # args: compute_cortical_thickness")
                       compute_cortical_thickness();  WEND

int                  W_clear_curvature  WBEGIN 
  ERR(1,"Wrong # args: clear_curvature")
                       clear_curvature();  WEND

int                  W_clear_ripflags  WBEGIN
  ERR(1,"Wrong # args: clear_ripflags")
                       clear_ripflags();  WEND

int                  W_restore_ripflags  WBEGIN
  ERR(2,"Wrong # args: restore_ripflags <mode>")
                       restore_ripflags(atoi(argv[1]));  WEND

int                  W_floodfill_marked_patch  WBEGIN
  ERR(2,"Wrong # args: floodfill_marked_patch <0=cutborder,1=fthreshborder,2=curvfill>")
                       floodfill_marked_patch(atoi(argv[1]));  WEND

int                  W_draw_curvature_line  WBEGIN
  ERR(1,"Wrong # args: draw_curvature_line")
                       draw_curvature_line();  WEND

int                  W_twocond  WBEGIN
  ERR(3,"Wrong # args: twocond <cond #0> <cond #1>")
                       twocond(atoi(argv[1]), atoi(argv[2]));  WEND

int                  W_cut_line  WBEGIN
  ERR(2,"Wrong # args: cut_line <0=open,1=closed>")
                       cut_line(atoi(argv[1]));  WEND

int                  W_plot_marked  WBEGIN
  ERR(2,"Wrong # args: plot_marked <file name>")
                       plot_marked(argv[1]);  WEND

int                  W_plot_curv  WBEGIN
  ERR(1,"Wrong # args: plot_curv <0=open,1=closed>")
                       plot_curv(atoi(argv[1]));  WEND

int                  W_draw_fundus  WBEGIN
  ERR(2,"Wrong # args: draw_fundus")
                       draw_fundus(fbnd_selected_boundary);  WEND

int                  W_put_retinotopy_stats_in_vals  WBEGIN
  ERR(1,"Wrong # args: put_retinotopy_stats_in_vals")
                       put_retinotopy_stats_in_vals();  WEND

int                  W_draw_vector  WBEGIN
  ERR(2,"Wrong # args: draw_vector <0=open,1=closed>")
                       draw_vector(argv[1]);  WEND

int                  W_cut_plane  WBEGIN
  ERR(1,"Wrong # args: cut_plane     [select 4 pnts: 3=>plane,1=>what to save]")
                       cut_plane();  WEND

int                  W_flatten  WBEGIN
  ERR(1,"Wrong # args: flatten")
                       flatten(tfname);  WEND

int                  W_normalize_binary_curv  WBEGIN 
  ERR(1,"Wrong # args: normalize_binary_curv")
                       normalize_binary_curvature();  WEND

int                  W_normalize_area  WBEGIN 
  ERR(1,"Wrong # args: normalize_area")
                       normalize_area();  WEND

int                  W_shift_values  WBEGIN
  ERR(1,"Wrong # args: shift_values")
                       shift_values();  WEND

int                  W_swap_values  WBEGIN 
  ERR(1,"Wrong # args: swap_values")
                       swap_values();  WEND

int                  W_swap_stat_val  WBEGIN
  ERR(1,"Wrong # args: swap_stat_val")
                       swap_stat_val();  WEND

int                  W_swap_val_val2  WBEGIN
  ERR(1,"Wrong # args: swap_val_val2")
                       swap_val_val2();  WEND

int                  W_compute_angles  WBEGIN
  ERR(1,"Wrong # args: compute_angles")
                       compute_angles();  WEND

int                  W_compute_fieldsign  WBEGIN
  ERR(1,"Wrong # args: compute_fieldsign")
                       compute_fieldsign();  WEND

int                  W_draw_radius WBEGIN
  ERR(1,"Wrong # args: draw_radius")
                       draw_spokes(RADIUS);  WEND

int                  W_draw_theta WBEGIN
  ERR(1,"Wrong # args: draw_theta")
                       draw_spokes(THETA);  WEND

int                  W_save_rgb  WBEGIN 
  ERR(1,"Wrong # args: save_rgb")
                       save_rgb(agfname);  WEND

int                  W_save_rgb_num  WBEGIN
  ERR(1,"Wrong # args: save_rgb_num")
                       save_rgb_num(gfname);  WEND

int                  W_save_rgb_named_orig  WBEGIN 
  ERR(2,"Wrong # args: save_rgb_named_orig <relfilename>")
                       save_rgb_named_orig(sgfname,argv[1]);  WEND

int                  W_save_rgb_cmp_frame  WBEGIN 
  ERR(1,"Wrong # args: save_rgb_cmp_frame")
                       save_rgb_cmp_frame(gfname,ilat);  WEND

int                  W_open_rgb_cmp_named  WBEGIN 
  ERR(2,"Wrong # args: open_rgb_cmp_named <filename>")
                       open_rgb_cmp_named(sgfname,argv[1]);  WEND

int                  W_save_rgb_cmp_frame_named  WBEGIN 
  ERR(2,"Wrong # args: save_rgb_cmp_frame_named <framelatency>")
                       save_rgb_cmp_frame_named(atof(argv[1]));  WEND

int                  W_close_rgb_cmp_named  WBEGIN 
  ERR(1,"Wrong # args: close_rgb_cmp_named")
                       close_rgb_cmp_named();  WEND

int                  W_rotate_brain_x  WBEGIN 
  ERR(2,"Wrong # args: rotate_brain_x <deg>")
                       rotate_brain(atof(argv[1]),'x'); WEND

int                  W_rotate_brain_y  WBEGIN 
  ERR(2,"Wrong # args: rotate_brain_y <deg>")
                       rotate_brain(atof(argv[1]),'y'); WEND

int                  W_rotate_brain_z  WBEGIN 
  ERR(2,"Wrong # args: rotate_brain_z <deg>")
                       rotate_brain(atof(argv[1]),'z'); WEND

int                  W_translate_brain_x  WBEGIN 
  ERR(2,"Wrong # args: translate_brain_x <mm>")
                       translate_brain(atof(argv[1]),0.0,0.0);  WEND

int                  W_translate_brain_y  WBEGIN
  ERR(2,"Wrong # args: translate_brain_y <mm>")
                       translate_brain(0.0,atof(argv[1]),0.0);  WEND

int                  W_translate_brain_z  WBEGIN
  ERR(2,"Wrong # args: translate_brain_z <mm>")
                       translate_brain(0.0,0.0,atof(argv[1]));  WEND

int                  W_scale_brain  WBEGIN
  ERR(2,"Wrong # args: scale_brain <mm>")
                       scale_brain(atof(argv[1]));  WEND

int                  W_resize_window  WBEGIN 
  ERR(2,"Wrong # args: resize_window <pix>")
                       resize_window(atoi(argv[1]));  WEND

int                  W_setsize_window  WBEGIN 
  ERR(2,"Wrong # args: setsize_window <pix>")
                       setsize_window(atoi(argv[1]));  WEND

int                  W_move_window  WBEGIN
  ERR(3,"Wrong # args: move_window <x> <y>")
                       move_window(atoi(argv[1]),atoi(argv[2]));  WEND

int                  W_do_lighting_model  WBEGIN
  ERR(6,"Wrong # args: do_lighting_model <lit0> <lit1> <lit2> <lit3> <offset>")
                       do_lighting_model(atof(argv[1]),atof(argv[2]),
                            atof(argv[3]),atof(argv[4]),atof(argv[5]));  WEND

int                  W_restore_zero_position  WBEGIN 
  ERR(1,"Wrong # args: restore_zero_position")
                       restore_zero_position();  WEND

int                  W_restore_initial_position  WBEGIN 
  ERR(1,"Wrong # args: restore_initial_position")
                       restore_initial_position();  WEND

int                  W_make_lateral_view  WBEGIN 
  ERR(1,"Wrong # args: make_lateral_view")
                       make_lateral_view(stem);  WEND

int                  W_make_lateral_view_second  WBEGIN 
  ERR(1,"Wrong # args: make_lateral_view_second")
                       make_lateral_view_second(stem);  WEND

int                  W_read_view_matrix  WBEGIN 
  ERR(1,"Wrong # args: read_view_matrix")
                       read_view_matrix(tfname);  WEND

int                  W_write_view_matrix  WBEGIN 
  ERR(1,"Wrong # args: write_view_matrix")
                       write_view_matrix(tfname);  WEND

int                  W_read_really_matrix  WBEGIN 
  ERR(1,"Wrong # args: read_really_matrix")
                       read_really_matrix(tfname);  WEND

int                  W_write_really_matrix  WBEGIN 
  ERR(1,"Wrong # args: write_really_matrix")
                       write_really_matrix(tfname);  WEND

int                  W_really_translate_brain  WBEGIN
  ERR(4,"Wrong # args: really_translate_brain <mm:rh/lh> <ant/post> <sup/inf>")
                       really_translate_brain(atof(argv[1]),atof(argv[2]),
                                                          atof(argv[3])); WEND
int                  W_really_scale_brain  WBEGIN
  ERR(4,"Wrong # args: really_scale_brain <rh/lh> <ant/post> <sup/inf>")
                       really_scale_brain(atof(argv[1]),atof(argv[2]),
                                                        atof(argv[3]));  WEND
int                  W_really_rotate_brain_x  WBEGIN
  ERR(2,"Wrong # args: really_rotate_brain_x <deg>    [N.B.: lh/rh axis]")
                       really_rotate_brain(atof(argv[1]),'x');  WEND

int                  W_align_sphere  WBEGIN
  ERR(1,"Wrong # args: align_sphere <deg>    [N.B.: lh/rh axis]")
                       align_sphere(mris);  WEND

int                  W_really_rotate_brain_y  WBEGIN
  ERR(2,"Wrong # args: really_rotate_brain_y <deg>    [N.B.: ant/post axis]")
                       really_rotate_brain(atof(argv[1]),'y');  WEND

int                  W_really_rotate_brain_z  WBEGIN
  ERR(2,"Wrong # args: really_rotate_brain_z <deg>    [N.B.: sup/inf axis]")
                       really_rotate_brain(atof(argv[1]),'z');  WEND

int                  W_really_center_brain  WBEGIN
  ERR(1,"Wrong # args: really_center_brain")
                       really_center_brain();  WEND

int                  W_really_center_second_brain  WBEGIN
  ERR(1,"Wrong # args: really_center_second_brain")
                       really_center_second_brain();  WEND

int                  W_really_align_brain  WBEGIN
  ERR(1,"Wrong # args: really_align_brain")
                       really_align_brain();  WEND

int                  W_read_binary_decimation  WBEGIN 
  ERR(1,"Wrong # args: read_binary_decimation")
                       read_binary_decimation(sfname);  WEND

int                  W_read_binary_dipoles  WBEGIN 
  ERR(1,"Wrong # args: read_binary_dipoles")
                       read_binary_dipoles(dfname);  WEND

int                  W_load_vals_from_sol  WBEGIN 
  ERR(4,"Wrong # args: load_vals_from_sol <tmid> <dt> <cond_num>")
                       load_vals_from_sol(atof(argv[1]),atof(argv[2]),
                                          atoi(argv[3]));  WEND

int                  W_load_var_from_sol  WBEGIN
  ERR(2,"Wrong # args: load_var_from_sol <cond_num>")
                       load_var_from_sol(atoi(argv[1]));  WEND

int                  W_read_iop  WBEGIN
  ERR(3,"Wrong # args: read_iop <iop_name> <hemi_num>")
                       read_iop(argv[1],atoi(argv[2]));  WEND

int                  W_read_rec  WBEGIN
  ERR(2,"Wrong # args: read_rec <rec_file>")
                       read_rec(argv[1]);  WEND

int                  W_read_ncov  WBEGIN
  ERR(2,"Wrong # args: read_ncov <ncov_file>")
                       read_ncov(argv[1]);  WEND

int                  W_filter_recs  WBEGIN
  ERR(1,"Wrong # args: filter_recs")
                       filter_recs();  WEND

int                  W_normalize_time_courses  WBEGIN 
  ERR(2,"Wrong # args: normalize_time_courses <type>")
                       normalize_time_courses(atoi(argv[1]));  WEND

int                  W_compute_timecourses  WBEGIN
  ERR(1,"Wrong # args: compute_timecourses")
                       compute_timecourses();  WEND

int                  W_normalize_inverse  WBEGIN
  ERR(1,"Wrong # args: normalize_inverse")
                       normalize_inverse();  WEND

int                  W_compute_pval_fwd  WBEGIN
  ERR(2,"Wrong # args: compute_pval_fwd <pvalthresh>")
                       compute_pval_fwd(atof(argv[1]));  WEND

int                  W_compute_select_fwd  WBEGIN
  ERR(2,"Wrong # args: compute_select_fwd <maxdist (mm)>")
                       compute_select_fwd(atof(argv[1]));  WEND

int                  W_compute_pval_inv  WBEGIN
  ERR(1,"Wrong # args: compute_pval_inv")
                       compute_pval_inv();  WEND

int                  W_find_orig_vertex_coordinates  WBEGIN 
  ERR(1,"Wrong # args: find_orig_vertex_coordinates")
                     find_orig_vertex_coordinates(selection);  WEND

int                  W_select_orig_vertex_coordinates  WBEGIN 
  ERR(1,"Wrong # args: select_orig_vertex_coordinates")
                       select_orig_vertex_coordinates(&selection);  WEND

int                  W_select_talairach_point  WBEGIN 
  ERR(4,"Wrong # args: select_talairach_point <xtal> <ytal> <ztal>")
                       select_talairach_point(&selection,
                         atof(argv[1]),atof(argv[2]),atof(argv[3]));  WEND

int                  W_read_orig_vertex_coordinates  WBEGIN
  ERR(1,"Wrong # args: read_orig_vertex_coordinates")
                       read_orig_vertex_coordinates(orfname);  WEND

int                  W_read_white_vertex_coordinates  WBEGIN
  ERR(1,"Wrong # args: read_white_vertex_coordinates")
                       read_white_vertex_coordinates();  WEND

int                  W_read_pial_vertex_coordinates  WBEGIN
  ERR(1,"Wrong # args: read_pial_vertex_coordinates")
                       read_pial_vertex_coordinates();  WEND

int                  W_read_canon_vertex_coordinates  WBEGIN
  ERR(2,"Wrong # args: read_canon_vertex_coordinates")
                       read_canon_vertex_coordinates(argv[1]);  WEND

int                  W_send_spherical_point  WBEGIN
  ERR(4,"Wrong # args: send_spherical_point <subject> <sphere> <orig>")
                       send_spherical_point(argv[1], argv[2], argv[3]);  WEND

int                  W_send_to_subject  WBEGIN
  ERR(2,"Wrong # args: send_to_subject <subject name>")
                       send_to_subject(argv[1]);  WEND

int                  W_resend_to_subject  WBEGIN
  ERR(1,"Wrong # args: resend_to_subject")
                       resend_to_subject();  WEND

int                  W_drawcb  WBEGIN
  ERR(1,"Wrong # args: drawcb")
                       drawcb();  WEND

int                  W_read_ellipsoid_vertex_coordinates  WBEGIN
  ERR(4,"Wrong # args: read_ellipsoid_vertex_coordinates <a> <b> <c>")
                       read_ellipsoid_vertex_coordinates(elfname,
                         atof(argv[1]),atof(argv[2]),atof(argv[3]));  WEND

int                  W_invert_surface  WBEGIN
  ERR(1,"Wrong # args: invert_surface ")
                       invert_surface();  WEND

int                  W_fix_nonzero_vals  WBEGIN
  ERR(1,"Wrong # args: fix_nonzero_vals ")
                       fix_nonzero_vals();  WEND

int                  W_invert_vertex  WBEGIN
  ERR(2,"Wrong # args: invert_vertex ")
                       invert_vertex(atoi(argv[1]));  WEND

int                  W_invert_face  WBEGIN
  ERR(2,"Wrong # args: invert_face ")
                       invert_face(atoi(argv[1]));  WEND

int                  W_mark_annotation  WBEGIN
  ERR(1,"Wrong # args: mark_annotation ")
                       mark_annotation(selection);  WEND

int                  W_mark_faces  WBEGIN
  ERR(2,"Wrong # args: mark_faces ")
                       mark_faces(atoi(argv[1]));  WEND

int                  W_mark_face  WBEGIN
  ERR(2,"Wrong # args: mark_face ")
                       mark_face(atoi(argv[1]));  WEND

int                  W_dump_vertex  WBEGIN
  ERR(2,"Wrong # args: dump_vertex ")
                       dump_vertex(atoi(argv[1]));  WEND

int                  W_val_to_mark  WBEGIN
  ERR(1,"Wrong # args: val_to_mark ")
                       val_to_mark();  WEND

int                  W_show_flat_regions  WBEGIN
  ERR(3,"Wrong # args: show_flat_regions ")
                       show_flat_regions(argv[1], atof(argv[2]));  WEND

int                  W_val_to_curv  WBEGIN
  ERR(1,"Wrong # args: val_to_curv ")
                       val_to_curv();  WEND

int                  W_val_to_stat  WBEGIN
  ERR(1,"Wrong # args: val_to_stat ")
                       val_to_stat();  WEND

int                  W_stat_to_val  WBEGIN
  ERR(1,"Wrong # args: stat_to_val ")
                       stat_to_val();  WEND


int                  W_f_to_p  WBEGIN
  ERR(3,"Wrong # args: f_to_p(numer_dof, denom_dof) ")
                       f_to_p(atoi(argv[1]), atoi(argv[2]));  WEND

int                  W_t_to_p  WBEGIN
  ERR(2,"Wrong # args: t_to_p(dof) ")
                       t_to_p(atoi(argv[1]));  WEND

int                  W_f_to_t  WBEGIN
  ERR(1,"Wrong # args: f_to_t ")
                       f_to_t();  WEND

int                  W_label_to_stat  WBEGIN
  ERR(1,"Wrong # args: label_to_stat ")
                       label_to_stat();  WEND

int                  W_remove_triangle_links  WBEGIN
  ERR(1,"Wrong # args: remove_triangle_links ")
                       remove_triangle_links();  WEND

int                  W_read_soltimecourse  WBEGIN
  ERR(2,"Wrong # args: read_soltimecourse ")
                       read_soltimecourse(argv[1]);  WEND

int                  W_read_imag_vals  WBEGIN
  ERR(2,"Wrong # args: read_imag_vals ")
                       read_imag_vals(argv[1]);  WEND

int                  W_sol_plot  WBEGIN
  ERR(3,"Wrong # args: sol_plot ")
                       sol_plot(atoi(argv[1]), atoi(argv[2]));  WEND

int                  W_read_curv_to_val  WBEGIN
  ERR(2,"Wrong # args: read_curv_to_val ")
                       read_curv_to_val(argv[1]);  WEND

int                  W_read_and_smooth_parcellation  WBEGIN
  ERR(5,"Wrong # args: read_and_smooth_parcellation ")
                       read_and_smooth_parcellation(argv[1],argv[2],
                                                    atoi(argv[3]),
                                                    atoi(argv[4]));  WEND

int                  W_read_parcellation  WBEGIN
  ERR(3,"Wrong # args: read_parcellation ")
                       read_parcellation(argv[1],argv[2]);  WEND

int                  W_read_disc  WBEGIN
  ERR(2,"Wrong # args: read_disc ")
                       read_disc(argv[1]);  WEND

int                  W_deconvolve_weights  WBEGIN
  ERR(3,"Wrong # args: deconvolve_weights(weight_fname, scale_fname) ")
                       deconvolve_weights(argv[1], argv[2]);  WEND

int                  W_curv_to_val  WBEGIN
  ERR(1,"Wrong # args: curv_to_val ")
                       curv_to_val();  WEND

int                  W_mask_label  WBEGIN
  ERR(2,"Wrong # args: mask_label ")
                       mask_label(argv[1]);  WEND

int                  W_dump_faces  WBEGIN
  ERR(2,"Wrong # args: dump_faces ")
                       dump_faces(atoi(argv[1]));  WEND

int                  W_orient_sphere  WBEGIN
  ERR(1,"Wrong # args: orient_sphere ")
                       orient_sphere();  WEND

int                  W_draw_ellipsoid_latlong  WBEGIN
  ERR(4,"Wrong # args: draw_ellipsoid_latlong <a> <b> <c>")
                       draw_ellipsoid_latlong(atof(argv[1]),atof(argv[2]),
                                                     atof(argv[3]));  WEND
int                  W_left_click WBEGIN 
  ERR(3,"Wrong # args: left_click <x> <y>      [relative to window 0,0]")
                       left_click(
                         (short)atoi(argv[1]),(short)atoi(argv[2])); WEND

int                  W_plot_all_time_courses WBEGIN
  ERR(1,"Wrong # args: plot_all_time_courses")
                       plot_all_time_courses();  WEND

int                  W_read_plot_list  WBEGIN
  ERR(2,"Wrong # args: read_plot_list <vlist_file> ")
                       read_plot_list(argv[1]);  WEND

int                  W_read_vertex_list  WBEGIN
  ERR(2,"Wrong # args: read_vertex_list <vlist_file> ")
                       read_vertex_list(argv[1]);  WEND

int                  W_draw_cursor  WBEGIN
  ERR(3,"Wrong # args: draw_cursor <vertex_number> <on_or_off>")
                       draw_cursor(atoi(argv[1]),atoi(argv[2]));  WEND

int                  W_mark_vertex  WBEGIN
  ERR(3,"Wrong # args: mark_vertex <vertex_number> <on_or_off>")
                       mark_vertex(atoi(argv[1]),atoi(argv[2]));  WEND

int                  W_draw_all_cursor  WBEGIN
  ERR(1,"Wrong # args: draw_all_cursor")
                       draw_all_cursor();  WEND

int                  W_draw_all_vertex_cursor  WBEGIN
  ERR(1,"Wrong # args: draw_all_vertex_cursor")
                       draw_all_vertex_cursor();  WEND

int                  W_clear_all_vertex_cursor  WBEGIN
  ERR(1,"Wrong # args: clear_all_vertex_cursor")
                       clear_all_vertex_cursor();  WEND
/* begin rkt */
int W_swap_vertex_fields WBEGIN
           ERR(3,"Wrong # args: swap_vertex_fields <typea> <typeb>")
     swap_vertex_fields(atoi(argv[1]),atoi(argv[2])); WEND
int W_clear_vertex_marks WBEGIN
           ERR(1,"Wrong # args: clear_vertex_marks")
     clear_vertex_marks(); WEND
int W_clear_all_vertex_marks WBEGIN
           ERR(1,"Wrong # args: clear_all_vertex_marks")
     clear_all_vertex_marks(); WEND

int W_close_marked_vertices WBEGIN
     ERR(1,"Wrong # args: close_marked_vertices")
     close_marked_vertices(); WEND

int W_undo_last_action WBEGIN
           ERR(1,"Wrong # args: undo_last_action")
     undo_do_first_action(); WEND

int                  W_sclv_read_binary_values  WBEGIN 
  ERR(2,"Wrong # args: sclv_read_binary_values field")
                       sclv_read_binary_values(vfname,atoi(argv[1]));  WEND

int                  W_sclv_read_binary_values_frame  WBEGIN 
  ERR(2,"Wrong # args: sclv_read_binary_values_frame field")
                       sclv_read_binary_values_frame(vfname,atoi(argv[1]));  WEND

int                  W_sclv_write_binary_values  WBEGIN 
  ERR(2,"Wrong # args: sclv_write_binary_values field")
                       sclv_write_binary_values(vfname,atoi(argv[1]));  WEND

int                  W_read_surface_vertex_set  WBEGIN
  ERR(3,"Wrong # args: read_surface_vertex_set file")
                       vset_read_vertex_set(atoi(argv[1]), argv[2]);  WEND

int                  W_set_current_vertex_set  WBEGIN
  ERR(2,"Wrong # args: set_current_vertex_set file")
                       vset_set_current_set(atoi(argv[1]));  WEND

int W_func_load_timecourse (ClientData clientData,Tcl_Interp *interp,
          int argc,char *argv[])
{
  if(argc!=3&&argc!=4)
    {
      Tcl_SetResult(interp,"Wrong # args: func_load_timecourse dir stem"
        " [registration]",TCL_VOLATILE);
      return TCL_ERROR;
    }
  
  if (argc==3)
      func_load_timecourse (argv[1],argv[2],NULL);
  /* even if we have 4 args, tcl could have passed us a blank string
     for the 4th. if it's blank, call the load function with a null
     registration, otherwise it will think it's a valid file name. */
  if (argc==4)
    {
      if (strcmp(argv[3],"")==0)
  func_load_timecourse (argv[1],argv[2],NULL);
      else
  func_load_timecourse (argv[1],argv[2],argv[3]);
    }
  return TCL_OK;
}

int W_func_load_timecourse_offset (ClientData clientData,Tcl_Interp *interp,
           int argc,char *argv[])
{
  if(argc!=2&&argc!=3)
    {
      Tcl_SetResult(interp,"Wrong # args: func_load_timecourse_offset dir stem"
        " [registration]",TCL_VOLATILE);
      return TCL_ERROR;
    }
  
  if (argc==2)
    func_load_timecourse_offset (argv[1],argv[2],NULL);
  if (argc==3)
    func_load_timecourse_offset (argv[1],argv[2],argv[3]);

  return TCL_OK;
}

int W_sclv_read_bfile_values (ClientData clientData,Tcl_Interp *interp,
            int argc,char *argv[])
{
  if(argc!=4&&argc!=5)
    {
      Tcl_SetResult(interp,"Wrong # args: read_bfile_values field dir stem "
        " [registration]",TCL_VOLATILE);
      return TCL_ERROR;
    }
  
  if (argc==4)
      sclv_read_bfile_values (argv[2],argv[3],NULL,atoi(argv[1]));
  /* even if we have 4 args, tcl could have passed us a blank string
     for the 4th. if it's blank, call the read function with a null
     registration, otherwise it will think it's a valid file name. */
  if (argc==5)
    {
      if (strcmp(argv[4],"")==0)
  sclv_read_bfile_values (argv[2],argv[3],NULL,atoi(argv[1]));
      else
  sclv_read_bfile_values (argv[2],argv[3],argv[4],atoi(argv[1]));
    }
  return TCL_OK;
}
int W_sclv_smooth  WBEGIN
     ERR(3,"Wrong # args: sclv_smooth steps field")
     sclv_smooth(atoi(argv[1]), atoi(argv[2]));  WEND
int W_sclv_set_current_field  WBEGIN
     ERR(2,"Wrong # args: sclv_set_current_field")
     sclv_set_current_field(atoi(argv[1]));  WEND
int W_sclv_set_current_timepoint  WBEGIN
     ERR(3,"Wrong # args: sclv_set_current_timepoint timepoint condition")
     sclv_set_timepoint_of_field(sclv_current_field,
         atoi(argv[1]),atoi(argv[2]));  WEND
int W_sclv_copy_view_settings_from_field  WBEGIN
     ERR(3,"Wrong # args: sclv_copy_view_settings_from_field field fromfield")
     sclv_copy_view_settings_from_field(atoi(argv[1]),atoi(argv[2]));  WEND
int W_sclv_copy_view_settings_from_current_field  WBEGIN
     ERR(2,"Wrong # args: sclv_copy_view_settings_from_current_field field")
     sclv_copy_view_settings_from_current_field(atoi(argv[1]));  WEND
int W_sclv_set_current_threshold_from_percentile  WBEGIN
     ERR(4,"Wrong # args: sclv_set_current_threshold_from_percentile thresh mid slope")
     sclv_set_current_threshold_from_percentile(atof(argv[1]),atof(argv[2]),atof(argv[3]));  WEND
int W_sclv_send_histogram  WBEGIN
     ERR(2,"Wrong # args: sclv_send_histogram field")
     sclv_send_histogram(atoi(argv[1]));  WEND
int W_sclv_get_normalized_color_for_value (ClientData clientData,
             Tcl_Interp *interp,
             int argc,char *argv[])
{
  float r, g, b;
  char stringResult[256];

  if(argc!=2)
    {
      Tcl_SetResult(interp,"Wrong # args: sclv_get_normalized_color_for_value "
        "value",TCL_VOLATILE);
      return TCL_ERROR;
    }

  /* get the rgb values here and print them to a string */
  sclv_get_normalized_color_for_value (sclv_current_field, 
               atof(argv[1]), &r, &g, &b);
  sprintf (stringResult, "%f %f %f", r, g, b);  
  
  /* set tcl result, volatile so tcl will make a copy of it. */
  Tcl_SetResult( interp, stringResult, TCL_VOLATILE );

  return TCL_OK;
}


int W_func_select_marked_vertices  WBEGIN
     ERR(1,"Wrong # args: func_select_marked_vertices")
     func_select_marked_vertices();  WEND
int W_func_select_label  WBEGIN
     ERR(1,"Wrong # args: func_select_label")
     func_select_label();  WEND
int W_func_clear_selection  WBEGIN
     ERR(1,"Wrong # args: func_clear_selection")
     func_clear_selection();  WEND
int W_func_graph_timecourse_selection  WBEGIN
     ERR(1,"Wrong # args: func_graph_timecourse_selection")
     func_graph_timecourse_selection();  WEND
int W_func_print_timecourse_selection  WBEGIN
     ERR(2,"Wrong # args: func_print_timecourse_selection filename")
     func_print_timecourse_selection(argv[1]);  WEND


int W_labl_load_color_table WBEGIN
     ERR(2,"Wrong # args: labl_load_color_table filename")
     labl_load_color_table (argv[1]); WEND
int W_labl_load WBEGIN
     ERR(2,"Wrong # args: labl_load filename")
     labl_load (argv[1]); WEND
int W_labl_save WBEGIN
     ERR(3,"Wrong # args: labl_save index filename")
     labl_save (atoi(argv[1]), argv[2]); WEND
int W_labl_save_all WBEGIN
     ERR(1,"Wrong # args: labl_save_all prefix")
     labl_save_all (argv[1]); WEND
int W_labl_import_annotation WBEGIN
     ERR(2,"Wrong # args: labl_import_annotation filename")
     labl_import_annotation (argv[1]); WEND
int W_labl_export_annotation WBEGIN
     ERR(2,"Wrong # args: labl_export_annotation filename")
     labl_export_annotation (argv[1]); WEND
int W_labl_new_from_marked_vertices WBEGIN
     ERR(1,"Wrong # args: labl_new_from_marked_vertices")
     labl_new_from_marked_vertices (); WEND
int W_labl_mark_vertices WBEGIN
     ERR(2,"Wrong # args: labl_mark_vertices index")
     labl_mark_vertices (atoi(argv[1])); WEND
int W_labl_select WBEGIN
     ERR(2,"Wrong # args: labl_select index")
     labl_select (atoi(argv[1])); WEND
int W_labl_set_name_from_table WBEGIN
     ERR(2,"Wrong # args: labl_set_name_from_table index")
     labl_set_name_from_table (atoi(argv[1])); WEND
int W_labl_set_info WBEGIN
     ERR(8,"Wrong # args: labl_set_info index name structure visibility")
     labl_set_info (atoi(argv[1]), argv[2], atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]), atoi(argv[7]) );WEND
int W_labl_set_color WBEGIN
     ERR(5,"Wrong # args: labl_set_color index r g b")
     labl_set_color (atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]) );WEND
int W_labl_remove WBEGIN
     ERR(2,"Wrong # args: labl_remove index")
     labl_remove (atoi(argv[1])); WEND
int W_labl_remove_all WBEGIN
     ERR(1,"Wrong # args: labl_remove")
     labl_remove_all (); WEND
int W_labl_select_label_by_vno WBEGIN
     ERR(2,"Wrong # args: labl_select_label_by_vno vno")
     labl_select_label_by_vno (atoi(argv[1])); WEND
int W_labl_print_list WBEGIN
     ERR(1,"Wrong # args: labl_print_list")
     labl_print_list(); WEND
int W_labl_print_table WBEGIN
     ERR(1,"Wrong # args: labl_print_table")
     labl_print_table(); WEND

int W_fbnd_select WBEGIN
     ERR(2,"Wrong # args: fbnd_select")
     fbnd_select( atoi(argv[1])); WEND
int W_fbnd_new_line_from_marked_vertices WBEGIN
     ERR(1,"Wrong # args: fbnd_new_line_from_marked_vertices")
     fbnd_new_line_from_marked_vertices(); WEND
int W_fbnd_remove_selected_boundary WBEGIN
     ERR(1,"Wrong # args: fbnd_remove_selected_boundary")
     fbnd_remove_selected_boundary(); WEND

int W_fill_flood_from_cursor (ClientData clientData,Tcl_Interp *interp,
            int argc,char *argv[])
{
  FILL_PARAMETERS params;

  if (argc != 5)
    {
      Tcl_SetResult(interp,"Wrong # args: fill_flood_from_cursor "
        "dont_cross_boundary dont_cross_label dont_cross_cmid "
        "dont_cross_fthresh",TCL_VOLATILE);
      return TCL_ERROR;
    }
  
  params.dont_cross_boundary = atoi(argv[1]);
  params.dont_cross_label = atoi(argv[2]);
  params.dont_cross_cmid = atoi(argv[3]);
  params.dont_cross_fthresh = atoi(argv[4]);
  fill_flood_from_seed (selection, &params);
  
  return TCL_OK;
}

/* end rkt */
/*=======================================================================*/

/* licensing */

#ifdef USE_LICENSE
#ifndef IRIX
extern char *crypt(char *, char *) ;
#endif
void checkLicense(char* dirname)
{
  FILE* lfile;
  char* email;
  char* magic;
  char* key;
  char* gkey;
  char* lfilename;

  lfilename = (char*)malloc(512);
  email = (char*)malloc(512);
  magic = (char*)malloc(512);
  key = (char*)malloc(512);
  gkey = (char*)malloc(1024);

  sprintf(lfilename,"%s/.license",dirname);

  lfile = fopen(lfilename,"r");
  if(lfile) {
    fscanf(lfile,"%s\n",email);
    fscanf(lfile,"%s\n",magic);
    fscanf(lfile,"%s\n",key);
    
    sprintf(gkey,"%s.%s",email,magic);
    if (strcmp(key,crypt(gkey,"*C*O*R*T*E*C*H*S*0*1*2*3*"))!=0) {
      printf("No valid license key !\n");
      exit(-1);
    }
  }
  else {
    printf("License file not found !\n");
    exit(-1);
  }
  free(email);
  free(magic);
  free(key);
  free(gkey);
  free(lfilename);
  return;  
}

#endif

/* for tcl/tk */
static void StdinProc _ANSI_ARGS_((ClientData clientData, int mask));
static void Prompt _ANSI_ARGS_((Tcl_Interp *interp, int partial));
#ifndef TCL8
static Tk_Window mainWindow;
#endif
static Tcl_Interp *interp;
static Tcl_DString command;
static int tty;

int main(int argc, char *argv[])   /* new main */
{
  int code;
  int scriptok=FALSE;
  int aliasflag=FALSE;
#ifndef TCL8
  static char *display = NULL;
#endif
  char tksurfer_tcl[NAME_LENGTH];
  char str[NAME_LENGTH];
  char script_tcl[NAME_LENGTH];
  char alias_tcl[NAME_LENGTH];
  char *envptr;
  FILE *fp;
#if defined(Linux) || defined(sun) || defined(SunOS)
  struct timeval tv;
#endif
  /* begin rkt */
  char tcl_cmd[1024];
  /* end rkt */

  Progname = argv[0] ;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  /* begin rkt */
  undo_initialize();
  vset_initialize();
  func_initialize();
  sclv_initialize();
  conv_initialize();
  labl_initialize();
  cncl_initialize();
  /* end rkt */

  /* get tksurfer tcl startup script location from environment */
  envptr = getenv("MRI_DIR");
  if (envptr==NULL) {
    printf("tksurfer: env var MRI_DIR undefined (use setenv)\n");
    printf("    [dir containing mri distribution]\n");
    exit(0);
  }

#ifdef USE_LICENSE
  checkLicense(envptr);
#endif

  /* begin rkt */

  /* check for a local tksurfer.new.tcl */
  strcpy(tksurfer_tcl,"tksurfer.new.tcl"); 
  if ((fp=fopen(tksurfer_tcl,"r"))!=NULL) 
    {
      printf("tksurfer: using new tksurfer.new.tcl\n");
      fclose(fp);
    }
  else 
    {
      /* check for a local tksurfer.tcl */
      strcpy(tksurfer_tcl,"tksurfer.tcl"); 
      if ((fp=fopen(tksurfer_tcl,"r"))!=NULL) 
  {
    printf("tksurfer: using LOCAL tksurfer.tcl\n");
    fclose(fp);
  }
      else 
  {
    
    /* look for tksurfer.new.tcl in mri dir */
    sprintf(tksurfer_tcl,"%s/lib/tcl/%s",envptr,"tksurfer.new.tcl"); 
    if ((fp=fopen(tksurfer_tcl,"r"))!=NULL) 
      {
        fclose(fp);
      }
    else 
      {
        
        /* look for tksurfer.tcl in mri dir */
        sprintf(tksurfer_tcl,"%s/lib/tcl/%s",envptr,"tksurfer.tcl"); 
        if ((fp=fopen(tksurfer_tcl,"r"))==NULL) 
    {
      printf("tksurfer: script %s not found\n",tksurfer_tcl);
      exit(0);
    }
        else {
    fclose(fp);
        }
      }
  }
    }
  /* end rkt */

  /* look for script: (1) cwd, (2) MRI_DIR/lib/tcl, (3) [same]/alias.tcl */
  sprintf(script_tcl,"%s/lib/tcl/twocond-views.tcl",envptr);  /* default */
  if (argc==6) 
  {
    strcpy(str,argv[4]);
    if (MATCH_STR("-tcl")) /* if command line script, run as batch job */
    {  
      char *cp ;

      if (argc!=6) 
      {
        printf("Usage: tksurfer [-]name hemi surf [-tcl script]\n");exit(0);
      }

      strcpy(script_tcl,argv[5]);
      fp = fopen(script_tcl,"r");  /* first, look in cwd */
      if (fp==NULL)                /* then MRI_DIR/lib/tcl dir */
      {
        sprintf(script_tcl,"%s/lib/tcl/%s",envptr,argv[5]);
        fp = fopen(script_tcl,"r");
        if (fp == NULL)            /* then TKSURFER_TCL_SCRIPTS dir */
        {
          cp = getenv("TKSURFER_TCL_SCRIPTS") ;
          if (cp)   /* see if script is in users has own scripts directory */
          {
            sprintf(script_tcl,"%s/%s",cp,argv[5]); 
            fp = fopen(script_tcl,"r"); 
          }
          else
            fp = NULL ;
          if (fp==NULL)              /* then aliases */
          {
            aliasflag = TRUE;
            sprintf(script_tcl,"%s/lib/tcl/alias.tcl",envptr); 
            fp = fopen(script_tcl,"r");
            if (fp==NULL)            /* couldn't find it anywhere */
            {
              printf("tksurfer: (1) File ./%s not found\n",argv[5]);
              printf("          (2) File %s/lib/tcl/%s not found\n",
                     envptr,argv[5]);
              printf("          (3) File %s/lib/tcl/alias.tcl not found\n",
                     envptr);
              exit(0);
            }
          }
        }
      }
      scriptok = TRUE;
    }
    else {
      ; /* ignore 6 arg command lines without -tcl option */
    }
  }

  /* start surfer, now as function; gl window not opened yet */
  /*  printf("tksurfer: starting surfer\n");*/
  Surfer((ClientData) NULL, interp, argc, argv); /* event loop commented out */

  /* start tcl/tk; first make interpreter */
  interp = Tcl_CreateInterp();
  /* begin rkt */
  g_interp = interp;
  /* end rkt */

  /* make main window (not displayed until event loop starts) */
  /*mainWindow = TkCreateMainWindow(interp, display, argv[0], "Tk");
  if (mainWindow == NULL) {
    fprintf(stderr, "%s\n", interp->result);
    exit(1); }
  */
  /* set the "tcl_interactive" variable */
  tty = isatty(0);
  Tcl_SetVar(interp, "tcl_interactive", (tty) ? "1" : "0", TCL_GLOBAL_ONLY);
  if (tty) promptflag = TRUE;  /* no-CR blocks pipe log read */

  /* read tcl/tk internal startup scripts */
  if (Tcl_Init(interp) == TCL_ERROR) {
    fprintf(stderr, "Tcl_Init failed: %s\n", interp->result); }
  if (Tk_Init(interp) == TCL_ERROR) {
    fprintf(stderr, "Tk_Init failed: %s\n", interp->result); }
  /* begin rkt */
  if (Tix_Init(interp) == TCL_ERROR) {
    fprintf(stderr, "Tix_Init failed: %s\n", interp->result); }
  if (Blt_Init(interp) == TCL_ERROR) {
    fprintf(stderr, "Blt_Init failed: %s\n", interp->result); }

  Tcl_StaticPackage( interp, "BLT", Blt_Init, Blt_SafeInit );
  Tcl_StaticPackage( interp, "Tix", Tix_Init, Tix_SafeInit );
  /* end rkt */

  /*=======================================================================*/
  /* register wrapped surfer functions with interpreter */
  Tcl_CreateCommand(interp, "swap_buffers",       W_swap_buffers,       REND);
  Tcl_CreateCommand(interp, "to_single_buffer",   W_to_single_buffer,   REND);
  Tcl_CreateCommand(interp, "to_double_buffer",   W_to_double_buffer,   REND);
  Tcl_CreateCommand(interp, "open_window",        W_open_window,        REND);
  Tcl_CreateCommand(interp, "help",               W_help,               REND);
  Tcl_CreateCommand(interp, "redraw",             W_redraw,             REND);
  Tcl_CreateCommand(interp, "redraw_second",      W_redraw_second,      REND);
  Tcl_CreateCommand(interp, "shrink",             W_shrink,             REND);
  Tcl_CreateCommand(interp, "area_shrink",        W_area_shrink,        REND);
  Tcl_CreateCommand(interp, "sphere_shrink",      W_sphere_shrink,      REND);
  Tcl_CreateCommand(interp, "ellipsoid_project",  W_ellipsoid_project,  REND);
  Tcl_CreateCommand(interp, "ellipsoid_morph",    W_ellipsoid_morph,     REND);
  Tcl_CreateCommand(interp, "ellipsoid_shrink",   W_ellipsoid_shrink,     REND);
  Tcl_CreateCommand(interp, "ellipsoid_shrink_bug",W_ellipsoid_shrink_bug,REND);
  Tcl_CreateCommand(interp, "curv_shrink_to_fill",W_curv_shrink_to_fill,REND);
  Tcl_CreateCommand(interp, "smooth_curvim",      W_smooth_curvim,      REND);
  Tcl_CreateCommand(interp, "smooth_curv",        W_smooth_curv,        REND);
  Tcl_CreateCommand(interp, "smooth_val",         W_smooth_val,         REND);
  Tcl_CreateCommand(interp, "smooth_val_sparse",  W_smooth_val_sparse,  REND);
  Tcl_CreateCommand(interp, "smooth_curvim_sparse",  
                           W_smooth_curvim_sparse,                      REND);
  Tcl_CreateCommand(interp, "smooth_fs",          W_smooth_fs,          REND);
  Tcl_CreateCommand(interp, "add_subject_to_average_curvim",
                           W_add_subject_to_average_curvim,REND);
  Tcl_CreateCommand(interp, "read_curv_images",   W_read_curv_images,   REND);
  Tcl_CreateCommand(interp, "read_stds",          W_read_stds,          REND);
  Tcl_CreateCommand(interp, "read_second_binary_surf",
                           W_read_second_binary_surf,                   REND);
  Tcl_CreateCommand(interp, "read_second_binary_curv",
                           W_read_second_binary_curv,                   REND);
  Tcl_CreateCommand(interp, "normalize_second_binary_curv",
                           W_normalize_second_binary_curv,              REND);
  Tcl_CreateCommand(interp, "curv_to_curvim",
                           W_curv_to_curvim,                            REND);
  Tcl_CreateCommand(interp, "second_surface_curv_to_curvim",
                           W_second_surface_curv_to_curvim,             REND);
  Tcl_CreateCommand(interp, "curvim_to_second_surface",
                           W_curvim_to_second_surface,                  REND);
  Tcl_CreateCommand(interp, "swap_curv",
                           W_swap_curv,                                 REND);
  Tcl_CreateCommand(interp, "curvim_to_surface",
                           W_curvim_to_surface,                         REND);
  Tcl_CreateCommand(interp, "read_binary_surf",   W_read_binary_surf,   REND);
  Tcl_CreateCommand(interp, "read_surf",               W_read_surf,   REND);
  Tcl_CreateCommand(interp, "save_surf",               W_save_surf,   REND);
  Tcl_CreateCommand(interp, "store_surf",               W_save_surf,   REND);
  Tcl_CreateCommand(interp, "restore_surf",               W_restore_surf,   REND);
  Tcl_CreateCommand(interp, "surf",               W_show_surf,   REND);
  Tcl_CreateCommand(interp, "read_binary_curv",   W_read_binary_curv,   REND);
  Tcl_CreateCommand(interp, "read_binary_sulc",   W_read_binary_sulc,   REND);
  Tcl_CreateCommand(interp, "read_binary_values", W_read_binary_values, REND);
  Tcl_CreateCommand(interp, "read_binary_values_frame", 
                           W_read_binary_values_frame,                  REND);
  Tcl_CreateCommand(interp, "read_annotated_image",W_read_annotated_image,REND);
  Tcl_CreateCommand(interp, "read_annotations",W_read_annotations,REND);
  Tcl_CreateCommand(interp, "read_binary_patch",  W_read_binary_patch,  REND);
  Tcl_CreateCommand(interp, "read_fieldsign",     W_read_fieldsign,     REND);
  Tcl_CreateCommand(interp, "read_fsmask",        W_read_fsmask,        REND);
  Tcl_CreateCommand(interp, "write_binary_areas", W_write_binary_areas, REND);
  Tcl_CreateCommand(interp, "write_binary_surface",W_write_binary_surface,REND);
  Tcl_CreateCommand(interp, "write_binary_curv",  W_write_binary_curv,REND);
  Tcl_CreateCommand(interp, "write_binary_sulc",  W_write_binary_sulc,REND);
  Tcl_CreateCommand(interp, "write_binary_values",W_write_binary_values,REND);
  Tcl_CreateCommand(interp, "write_binary_patch", W_write_binary_patch, REND);
  Tcl_CreateCommand(interp, "write_labeled_vertices", 
                           W_write_labeled_vertices,                    REND);
  Tcl_CreateCommand(interp, "read_labeled_vertices", 
                           W_read_labeled_vertices,                    REND);
  Tcl_CreateCommand(interp, "read_and_color_labeled_vertices", 
                           W_read_and_color_labeled_vertices,           REND);
  Tcl_CreateCommand(interp, "write_fieldsign",    W_write_fieldsign,    REND);
  Tcl_CreateCommand(interp, "write_fsmask",       W_write_fsmask,       REND);
  Tcl_CreateCommand(interp, "write_vrml",         W_write_vrml,         REND);
  Tcl_CreateCommand(interp, "write_binary_dipoles",W_write_binary_dipoles,REND);
  Tcl_CreateCommand(interp, "write_binary_decimation",
                           W_write_binary_decimation,                   REND);
  Tcl_CreateCommand(interp, "write_dipoles",      W_write_dipoles,      REND);
  Tcl_CreateCommand(interp, "write_decimation",   W_write_decimation,   REND);
  Tcl_CreateCommand(interp, "write_curv_images",  W_write_curv_images,  REND);
  Tcl_CreateCommand(interp, "write_fill_images",  W_write_fill_images,  REND);
  Tcl_CreateCommand(interp, "fill_second_surface",W_fill_second_surface,REND);
  Tcl_CreateCommand(interp, "subsample_dist",     W_subsample_dist,     REND);
  Tcl_CreateCommand(interp, "subsample_orient",   W_subsample_orient,   REND);
  Tcl_CreateCommand(interp, "write_subsample",    W_write_subsample,    REND);
  Tcl_CreateCommand(interp, "compute_curvature",  W_compute_curvature,  REND);
  Tcl_CreateCommand(interp, "compute_CMF",        W_compute_CMF,        REND);
  Tcl_CreateCommand(interp, "compute_cortical_thickness", 
                           W_compute_cortical_thickness,                REND);
  Tcl_CreateCommand(interp, "clear_curvature",    W_clear_curvature,    REND);
  Tcl_CreateCommand(interp, "clear_ripflags",     W_clear_ripflags,     REND);
  Tcl_CreateCommand(interp, "restore_ripflags",   W_restore_ripflags,   REND);
  Tcl_CreateCommand(interp, "floodfill_marked_patch",
                           W_floodfill_marked_patch,                    REND);
  Tcl_CreateCommand(interp, "twocond",
                           W_twocond,                    REND);
  Tcl_CreateCommand(interp, "cut_line",           W_cut_line,           REND);
  Tcl_CreateCommand(interp, "plot_curv",          W_plot_curv,          REND);
  Tcl_CreateCommand(interp, "draw_fundus",        W_draw_fundus,        REND);
  Tcl_CreateCommand(interp, "plot_marked",        W_plot_marked,        REND);
  Tcl_CreateCommand(interp, "put_retinotopy_stats_in_vals",          
                    W_put_retinotopy_stats_in_vals,                     REND);
  Tcl_CreateCommand(interp, "draw_vector",        W_draw_vector,        REND);
  Tcl_CreateCommand(interp, "cut_plane",          W_cut_plane,          REND);
  Tcl_CreateCommand(interp, "flatten",            W_flatten,            REND);
  Tcl_CreateCommand(interp, "normalize_binary_curv",
                           W_normalize_binary_curv,                    REND);
  Tcl_CreateCommand(interp, "normalize_area",     W_normalize_area,     REND);
  Tcl_CreateCommand(interp, "shift_values",       W_shift_values,       REND);
  Tcl_CreateCommand(interp, "swap_values",        W_swap_values,        REND);
  Tcl_CreateCommand(interp, "swap_stat_val",      W_swap_stat_val,      REND);
  Tcl_CreateCommand(interp, "swap_val_val2",      W_swap_val_val2,      REND);
  Tcl_CreateCommand(interp, "compute_angles",     W_compute_angles,     REND);
  Tcl_CreateCommand(interp, "compute_fieldsign",  W_compute_fieldsign,  REND);
  Tcl_CreateCommand(interp, "draw_radius",        W_draw_radius,        REND);
  Tcl_CreateCommand(interp, "draw_theta",         W_draw_theta,         REND);
  Tcl_CreateCommand(interp, "save_rgb",           W_save_rgb,           REND);
  Tcl_CreateCommand(interp, "save_rgb_named_orig",W_save_rgb_named_orig,REND);
  Tcl_CreateCommand(interp, "save_rgb_cmp_frame", W_save_rgb_cmp_frame, REND);
  Tcl_CreateCommand(interp, "open_rgb_cmp_named", W_open_rgb_cmp_named, REND);
  Tcl_CreateCommand(interp, "save_rgb_cmp_frame_named",
                           W_save_rgb_cmp_frame_named,                  REND);
  Tcl_CreateCommand(interp, "close_rgb_cmp_named",W_close_rgb_cmp_named,REND);
  Tcl_CreateCommand(interp, "rotate_brain_x",     W_rotate_brain_x,     REND);
  Tcl_CreateCommand(interp, "rotate_brain_y",     W_rotate_brain_y,     REND);
  Tcl_CreateCommand(interp, "rotate_brain_z",     W_rotate_brain_z,     REND);
  Tcl_CreateCommand(interp, "translate_brain_x",  W_translate_brain_x,  REND);
  Tcl_CreateCommand(interp, "translate_brain_y",  W_translate_brain_y,  REND);
  Tcl_CreateCommand(interp, "translate_brain_z",  W_translate_brain_z,  REND);
  Tcl_CreateCommand(interp, "scale_brain",        W_scale_brain,        REND);
  Tcl_CreateCommand(interp, "resize_window",      W_resize_window,      REND);
  Tcl_CreateCommand(interp, "setsize_window",     W_setsize_window,     REND);
  Tcl_CreateCommand(interp, "move_window",        W_move_window,        REND);
  Tcl_CreateCommand(interp, "do_lighting_model",  W_do_lighting_model,  REND);
  Tcl_CreateCommand(interp, "restore_zero_position",
                           W_restore_zero_position,                     REND);
  Tcl_CreateCommand(interp, "restore_initial_position",
                           W_restore_initial_position,                  REND);
  Tcl_CreateCommand(interp, "make_lateral_view",  W_make_lateral_view,  REND);
  Tcl_CreateCommand(interp, "make_lateral_view_second", 
                           W_make_lateral_view_second,                  REND);
  Tcl_CreateCommand(interp, "read_view_matrix",   W_read_view_matrix,   REND);
  Tcl_CreateCommand(interp, "write_view_matrix",  W_write_view_matrix,  REND);
  Tcl_CreateCommand(interp, "read_really_matrix", W_read_really_matrix, REND);
  Tcl_CreateCommand(interp, "write_really_matrix",W_write_really_matrix,REND);
  Tcl_CreateCommand(interp, "really_translate_brain",
                           W_really_translate_brain,                    REND);
  Tcl_CreateCommand(interp, "really_scale_brain", W_really_scale_brain, REND);
  Tcl_CreateCommand(interp, "align_sphere", 
                           W_align_sphere,                              REND);
  Tcl_CreateCommand(interp, "really_rotate_brain_x", 
                           W_really_rotate_brain_x,                     REND);
  Tcl_CreateCommand(interp, "really_rotate_brain_y", 
                           W_really_rotate_brain_y,                     REND);
  Tcl_CreateCommand(interp, "really_rotate_brain_z", 
                           W_really_rotate_brain_z,                     REND);
  Tcl_CreateCommand(interp, "really_center_brain",W_really_center_brain, REND);
  Tcl_CreateCommand(interp, "really_center_second_brain",
                           W_really_center_second_brain, REND);
  Tcl_CreateCommand(interp, "really_align_brain", W_really_align_brain, REND);
  Tcl_CreateCommand(interp, "read_binary_decimation", 
                           W_read_binary_decimation,                    REND);
  Tcl_CreateCommand(interp, "read_binary_dipoles", 
                           W_read_binary_dipoles,                       REND);
  Tcl_CreateCommand(interp, "load_vals_from_sol", 
                           W_load_vals_from_sol,                        REND);
  Tcl_CreateCommand(interp, "load_var_from_sol",
                           W_load_var_from_sol,                         REND);
  Tcl_CreateCommand(interp, "compute_timecourses",W_compute_timecourses,
                                                                        REND);
  Tcl_CreateCommand(interp, "filter_recs",W_filter_recs,                REND);
  Tcl_CreateCommand(interp, "read_rec",W_read_rec,                      REND);
  Tcl_CreateCommand(interp, "read_iop",W_read_iop,                      REND);
  Tcl_CreateCommand(interp, "read_ncov",W_read_ncov,                    REND);
  Tcl_CreateCommand(interp, "normalize_time_courses", 
                           W_normalize_time_courses,                    REND);
  Tcl_CreateCommand(interp, "compute_timecourses",W_compute_timecourses,
                                                                        REND);
  Tcl_CreateCommand(interp, "compute_pval_fwd",W_compute_pval_fwd,      REND);
  Tcl_CreateCommand(interp, "compute_select_fwd",W_compute_select_fwd,  REND);
  Tcl_CreateCommand(interp, "compute_pval_inv",W_compute_pval_inv,      REND);
  Tcl_CreateCommand(interp, "normalize_inverse",
                           W_normalize_inverse,                         REND);
  Tcl_CreateCommand(interp, "find_orig_vertex_coordinates",
                           W_find_orig_vertex_coordinates,              REND);
  Tcl_CreateCommand(interp, "select_orig_vertex_coordinates",
                           W_select_orig_vertex_coordinates,            REND);
  Tcl_CreateCommand(interp, "select_talairach_point",
                           W_select_talairach_point,                    REND);
  Tcl_CreateCommand(interp, "read_white_vertex_coordinates",
                           W_read_white_vertex_coordinates,              REND);
  Tcl_CreateCommand(interp, "read_pial_vertex_coordinates",
                           W_read_pial_vertex_coordinates,              REND);
  Tcl_CreateCommand(interp, "read_orig_vertex_coordinates",
                           W_read_orig_vertex_coordinates,              REND);
  Tcl_CreateCommand(interp, "read_canon_vertex_coordinates",
                           W_read_canon_vertex_coordinates,              REND);
  Tcl_CreateCommand(interp, "send_spherical_point",
                           W_send_spherical_point,              REND);
  Tcl_CreateCommand(interp, "send_to_subject",
                           W_send_to_subject,              REND);
  Tcl_CreateCommand(interp, "resend_to_subject",
                           W_resend_to_subject,              REND);
  Tcl_CreateCommand(interp, "drawcb",
                           W_drawcb,              REND);
  Tcl_CreateCommand(interp, "read_ellipsoid_vertex_coordinates",
                           W_read_ellipsoid_vertex_coordinates,         REND);
  Tcl_CreateCommand(interp, "invert_surface",
                           W_invert_surface,         REND);
  Tcl_CreateCommand(interp, "fix_nonzero_vals",
                           W_fix_nonzero_vals,         REND);
  Tcl_CreateCommand(interp, "invert_vertex",
                           W_invert_vertex,         REND);
  Tcl_CreateCommand(interp, "invert_face",
                           W_invert_face,         REND);
  Tcl_CreateCommand(interp, "mark_annotation",
                           W_mark_annotation,         REND);
  Tcl_CreateCommand(interp, "mark_faces",
                           W_mark_faces,         REND);
  Tcl_CreateCommand(interp, "mark_face",
                           W_mark_face,         REND);
  Tcl_CreateCommand(interp, "dump_vertex",
                           W_dump_vertex,         REND);
  Tcl_CreateCommand(interp, "val_to_mark",
                           W_val_to_mark,         REND);
  Tcl_CreateCommand(interp, "show_flat_regions",
                           W_show_flat_regions,         REND);
  Tcl_CreateCommand(interp, "val_to_stat",
                           W_val_to_stat,         REND);
  Tcl_CreateCommand(interp, "stat_to_val",
                           W_stat_to_val,         REND);
  Tcl_CreateCommand(interp, "read_soltimecourse",
                           W_read_soltimecourse,         REND);
  Tcl_CreateCommand(interp, "read_imag_vals",
                           W_read_imag_vals,         REND);
  Tcl_CreateCommand(interp, "sol_plot",
                           W_sol_plot,         REND);
  Tcl_CreateCommand(interp, "remove_triangle_links",
                           W_remove_triangle_links,         REND);
  Tcl_CreateCommand(interp, "f_to_t",
                           W_f_to_t,         REND);
  Tcl_CreateCommand(interp, "label_to_stat",
                           W_label_to_stat,         REND);
  Tcl_CreateCommand(interp, "t_to_p",
                           W_t_to_p,         REND);
  Tcl_CreateCommand(interp, "f_to_p",
                           W_f_to_p,         REND);
  Tcl_CreateCommand(interp, "val_to_curv",
                           W_val_to_curv,         REND);
  Tcl_CreateCommand(interp, "curv_to_val",
                           W_curv_to_val,         REND);
  Tcl_CreateCommand(interp, "read_curv_to_val",
                           W_read_curv_to_val,         REND);
  Tcl_CreateCommand(interp, "read_and_smooth_parcellation",
                           W_read_and_smooth_parcellation,         REND);
  Tcl_CreateCommand(interp, "read_parcellation",
                           W_read_parcellation,         REND);
  Tcl_CreateCommand(interp, "deconvolve_weights",
                           W_deconvolve_weights,         REND);
  Tcl_CreateCommand(interp, "read_disc",
                           W_read_disc,         REND);
  Tcl_CreateCommand(interp, "mask_label",
                           W_mask_label,         REND);
  Tcl_CreateCommand(interp, "orient_sphere",
                           W_orient_sphere,         REND);
  Tcl_CreateCommand(interp, "dump_faces",
                           W_dump_faces,         REND);
  Tcl_CreateCommand(interp, "draw_ellipsoid_latlong",
                           W_draw_ellipsoid_latlong,                    REND);
  Tcl_CreateCommand(interp, "left_click", W_left_click,                 REND);
  Tcl_CreateCommand(interp, "plot_all_time_courses",
                           W_plot_all_time_courses,      REND);
  Tcl_CreateCommand(interp, "read_plot_list",
                           W_read_plot_list,      REND);
  Tcl_CreateCommand(interp, "read_vertex_list",
                           W_read_vertex_list,      REND);
  Tcl_CreateCommand(interp, "draw_cursor",
                           W_draw_cursor,      REND);
  Tcl_CreateCommand(interp, "mark_vertex",
                           W_mark_vertex,      REND);
  Tcl_CreateCommand(interp, "draw_all_cursor",
                           W_draw_all_cursor,      REND);
  Tcl_CreateCommand(interp, "draw_all_vertex_cursor",
                           W_draw_all_vertex_cursor,      REND);
  Tcl_CreateCommand(interp, "clear_all_vertex_cursor",
                           W_clear_all_vertex_cursor,      REND);
  /* begin rkt */
  Tcl_CreateCommand(interp, "swap_vertex_fields",
              W_swap_vertex_fields, REND);

  Tcl_CreateCommand(interp, "clear_vertex_marks",
              W_clear_vertex_marks, REND);
  Tcl_CreateCommand(interp, "clear_all_vertex_marks",
              W_clear_all_vertex_marks, REND);

  Tcl_CreateCommand(interp, "close_marked_vertices",
        W_close_marked_vertices, REND);

  Tcl_CreateCommand(interp, "undo_last_action",
        W_undo_last_action, REND);

  Tcl_CreateCommand(interp, "sclv_read_binary_values", 
        W_sclv_read_binary_values, REND);
  Tcl_CreateCommand(interp, "sclv_read_binary_values_frame", 
        W_sclv_read_binary_values_frame, REND);
  Tcl_CreateCommand(interp, "sclv_read_bfile_values", 
        W_sclv_read_bfile_values, REND);
  Tcl_CreateCommand(interp, "sclv_write_binary_values", 
        W_sclv_write_binary_values, REND);
  Tcl_CreateCommand(interp, "sclv_smooth", 
        W_sclv_smooth, REND);
  Tcl_CreateCommand(interp, "sclv_set_current_field", 
        W_sclv_set_current_field, REND);
  Tcl_CreateCommand(interp, "sclv_set_current_timepoint", 
        W_sclv_set_current_timepoint, REND);
  Tcl_CreateCommand(interp, "sclv_copy_view_settings_from_current_field", 
        W_sclv_copy_view_settings_from_current_field, REND);
  Tcl_CreateCommand(interp, "sclv_copy_view_settings_from_field", 
        W_sclv_copy_view_settings_from_field, REND);
  Tcl_CreateCommand(interp, "sclv_set_current_threshold_from_percentile", 
        W_sclv_set_current_threshold_from_percentile, REND);
  Tcl_CreateCommand(interp, "sclv_send_histogram",
        W_sclv_send_histogram, REND);
  Tcl_CreateCommand(interp, "sclv_get_normalized_color_for_value",
        W_sclv_get_normalized_color_for_value, REND);

  Tcl_CreateCommand(interp, "read_surface_vertex_set",   
        W_read_surface_vertex_set,   REND);
  Tcl_CreateCommand(interp, "set_current_vertex_set",   
        W_set_current_vertex_set,   REND);

  Tcl_CreateCommand(interp, "func_load_timecourse",
        W_func_load_timecourse, REND);
  Tcl_CreateCommand(interp, "func_load_timecourse_offset",
        W_func_load_timecourse_offset, REND);
  Tcl_CreateCommand(interp, "func_select_marked_vertices",
        W_func_select_marked_vertices, REND);
  Tcl_CreateCommand(interp, "func_select_label",
        W_func_select_label, REND);
  Tcl_CreateCommand(interp, "func_clear_selection",
        W_func_clear_selection, REND);
  Tcl_CreateCommand(interp, "func_graph_timecourse_selection",
        W_func_graph_timecourse_selection, REND);
  Tcl_CreateCommand(interp, "func_print_timecourse_selection",
        W_func_print_timecourse_selection, REND);
  
  Tcl_CreateCommand(interp, "labl_load_color_table",
  W_labl_load_color_table, REND);
  Tcl_CreateCommand(interp, "labl_load",
  W_labl_load, REND);
  Tcl_CreateCommand(interp, "labl_save",
  W_labl_save, REND);
  Tcl_CreateCommand(interp, "labl_save_all",
  W_labl_save_all, REND);
  Tcl_CreateCommand(interp, "labl_import_annotation",
  W_labl_import_annotation, REND);
  Tcl_CreateCommand(interp, "labl_export_annotation",
  W_labl_export_annotation, REND);
  Tcl_CreateCommand(interp, "labl_new_from_marked_vertices",
  W_labl_new_from_marked_vertices, REND);
  Tcl_CreateCommand(interp, "labl_mark_vertices",
  W_labl_mark_vertices, REND);
  Tcl_CreateCommand(interp, "labl_select",
  W_labl_select, REND);
  Tcl_CreateCommand(interp, "labl_set_name_from_table",
        W_labl_set_name_from_table, REND);
  Tcl_CreateCommand(interp, "labl_set_info",
        W_labl_set_info, REND);
  Tcl_CreateCommand(interp, "labl_set_color",
        W_labl_set_color, REND);
  Tcl_CreateCommand(interp, "labl_remove",
  W_labl_remove, REND);
  Tcl_CreateCommand(interp, "labl_remove_all",
  W_labl_remove_all, REND);
  Tcl_CreateCommand(interp, "labl_select_label_by_vno",
  W_labl_select_label_by_vno, REND);
  Tcl_CreateCommand(interp, "labl_print_list",
        W_labl_print_list, REND);
  Tcl_CreateCommand(interp, "labl_print_table",
        W_labl_print_table, REND);

  Tcl_CreateCommand(interp, "fbnd_select",
        W_fbnd_select, REND);
  Tcl_CreateCommand(interp, "fbnd_new_line_from_marked_vertices",
        W_fbnd_new_line_from_marked_vertices, REND);
  Tcl_CreateCommand(interp, "fbnd_remove_selected_boundary",
        W_fbnd_remove_selected_boundary, REND);

  Tcl_CreateCommand(interp, "fill_flood_from_cursor",
        W_fill_flood_from_cursor, REND);

  Tcl_CreateCommand(interp, "draw_curvature_line",
                    W_draw_curvature_line, REND);

  /* end rkt */
  /*=======================================================================*/
  /***** link global surfer BOOLEAN variables to tcl equivalents */
  Tcl_LinkVar(interp,"use_vertex_arrays",(char *)&use_vertex_arrays, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"use_display_lists",(char *)&use_display_lists, TCL_LINK_BOOLEAN);
  
  Tcl_LinkVar(interp,"dlat",(char *)&dlat, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"momentumflag",(char *)&momentumflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"expandflag",(char *)&expandflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"MRIflag",(char *)&MRIflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"sulcflag",(char *)&sulcflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"avgflag",(char *)&avgflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"areaflag",(char *)&areaflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"complexvalflag",(char *)&complexvalflag,TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"statflag",(char *)&statflag,TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"overlayflag",(char *)&overlayflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"ncthreshflag",(char *)&ncthreshflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"verticesflag",(char *)&verticesflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"shearvecflag",(char *)&shearvecflag,TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"normvecflag",(char *)&normvecflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"movevecflag",(char *)&movevecflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"autoscaleflag",(char *)&autoscaleflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"revfsflag",(char *)&revfsflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"revphaseflag",(char *)&revphaseflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"invphaseflag",(char *)&invphaseflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"rectphaseflag",(char *)&rectphaseflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"truncphaseflag",(char *)&truncphaseflag,TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"fieldsignflag",(char *)&fieldsignflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"flag2d",(char *)&flag2d, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"scalebarflag",(char *)&scalebarflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"colscalebarflag",(char *)&colscalebarflag,
                                                     TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"pointsflag",(char *)&pointsflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"surfaceflag",(char *)&surfaceflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"phasecontourflag",(char *)&phasecontourflag,
                                                     TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"curvimflag",(char *)&curvimflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"doublebufferflag",(char *)&doublebufferflag,
                                                     TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"surface_compiled",(char *)&surface_compiled,
                                                     TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"openglwindowflag",(char *)&openglwindowflag,
                                                     TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"blinkflag",(char *)&blinkflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"renderoffscreen",(char *)&renderoffscreen,
                                                     TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"transform_loaded",(char *)&transform_loaded,
                                                     TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"blackcursorflag",(char *)&blackcursorflag,
                                                     TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"bigcursorflag",(char *)&bigcursorflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"vrml2flag",(char *)&vrml2flag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"showorigcoordsflag",(char *)&showorigcoordsflag,
                                                     TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"phasecontourmodflag",(char *)&phasecontourmodflag,
                                                     TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"promptflag",(char *)&promptflag,TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"followglwinflag",(char *)&followglwinflag,
                                                     TCL_LINK_BOOLEAN);
  /* begin rkt */
  Tcl_LinkVar(interp,"curvflag",(char *)&curvflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"mouseoverflag",(char *)&mouseoverflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"simpledrawmodeflag",(char *)&simpledrawmodeflag, TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"redrawlockflag",(char *)&redrawlockflag,
                                               TCL_LINK_BOOLEAN);
  Tcl_LinkVar(interp,"drawlabelflag",(char *)&labl_draw_flag,
        TCL_LINK_BOOLEAN);
  /* end rkt */
  /*=======================================================================*/
  /***** link global surfer INT variables to tcl equivalents */
  Tcl_LinkVar(interp,"nperdip",(char *)&sol_nperdip, TCL_LINK_INT);
  Tcl_LinkVar(interp,"scrsaveflag",(char *)&scrsaveflag, TCL_LINK_INT);
  Tcl_LinkVar(interp,"surfcolor",(char *)&surfcolor, TCL_LINK_INT);
  Tcl_LinkVar(interp,"mingm",(char *)&mingm, TCL_LINK_INT);
  Tcl_LinkVar(interp,"colscale",(char *)&colscale, TCL_LINK_INT);
  Tcl_LinkVar(interp,"ilat",(char *)&ilat, TCL_LINK_INT);
  Tcl_LinkVar(interp,"mesh_linewidth",(char *)&mesh_linewidth, TCL_LINK_INT);
  Tcl_LinkVar(interp,"meshr",(char *)&meshr, TCL_LINK_INT);
  Tcl_LinkVar(interp,"meshg",(char *)&meshg, TCL_LINK_INT);
  Tcl_LinkVar(interp,"meshb",(char *)&meshb, TCL_LINK_INT);
  Tcl_LinkVar(interp,"scalebar_bright",(char *)&scalebar_bright, TCL_LINK_INT);
  Tcl_LinkVar(interp,"project",(char *)&project, TCL_LINK_INT);
  Tcl_LinkVar(interp,"sol_plot_type",(char *)&sol_plot_type, TCL_LINK_INT);
  Tcl_LinkVar(interp,"phasecnntour_bright",(char *)&phasecontour_bright, 
                                                               TCL_LINK_INT);
  Tcl_LinkVar(interp,"blinkdelay",(char *)&blinkdelay, TCL_LINK_INT);
  Tcl_LinkVar(interp,"blinktime",(char *)&blinktime, TCL_LINK_INT);
  Tcl_LinkVar(interp,"select",(char *)&selection, TCL_LINK_INT);

  /* begin rkt */
  Tcl_LinkVar(interp,"vertexset",(char *)&vset_current_set, TCL_LINK_INT);
  Tcl_LinkVar(interp,"numprestimpoints",(char *)&func_num_prestim_points, TCL_LINK_INT);
  Tcl_LinkVar(interp,"currentvaluefield",(char *)&sclv_current_field,
                                               TCL_LINK_INT);
  Tcl_LinkVar(interp,"ftimepoint",(char *)&sclv_cur_timepoint, TCL_LINK_INT);
  Tcl_LinkVar(interp,"fcondition",(char *)&sclv_cur_condition, TCL_LINK_INT);
  Tcl_LinkVar(interp,"fnumtimepoints",(char *)&sclv_num_timepoints, TCL_LINK_INT);
  Tcl_LinkVar(interp,"fnumconditions",(char *)&sclv_num_conditions, TCL_LINK_INT);
  Tcl_LinkVar(interp,"labelstyle",(char *)&labl_draw_style, TCL_LINK_INT);
  /* end rkt */
  /*=======================================================================*/
  /***** link global surfer DOUBLE variables to tcl equivalents (were float) */
  Tcl_LinkVar(interp,"decay",(char *)&decay, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"fthresh",(char *)&fthresh, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"fslope",(char *)&fslope, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"fcurv",(char *)&fcurv, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"foffset",(char *)&foffset, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"fmid",(char *)&fmid, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"cslope",(char *)&cslope, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"cmid",(char *)&cmid, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"cmax",(char *)&cmax, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"cmin",(char *)&cmin, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"mslope",(char *)&mslope, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"mmid",(char *)&mmid, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"whitemid",(char *)&whitemid, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"graymid",(char *)&graymid, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"mstrength",(char *)&mstrength, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"stressthresh",(char *)&stressthresh, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"icstrength",(char *)&icstrength, TCL_LINK_DOUBLE);
  /*Tcl_LinkVar(interp,"dipscale",(char *)&dipscale, TCL_LINK_DOUBLE);*/
  Tcl_LinkVar(interp,"angle_cycles",(char *)&angle_cycles, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"angle_offset",(char *)&angle_offset, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"wt",(char *)&wt, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"wa",(char *)&wa, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"ws",(char *)&ws, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"wn",(char *)&wn, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"wc",(char *)&wc, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"wsh",(char *)&wsh, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"wbn",(char *)&wbn, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"update",(char *)&update, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"decay",(char *)&decay, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"cthk",(char *)&cthk, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"light0",(char *)&light0_br, TCL_LINK_DOUBLE); /* suffix */
  Tcl_LinkVar(interp,"light1",(char *)&light1_br, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"light2",(char *)&light2_br, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"light3",(char *)&light3_br, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"offset",(char *)&offset, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"blufact",(char *)&blufact, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"cvfact",(char *)&cvfact, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"fadef",(char *)&fadef, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"ncthresh",(char *)&ncthresh, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"dipavg",(char *)&dipavg, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"scalebar_xpos",(char *)&scalebar_xpos, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"scalebar_ypos",(char *)&scalebar_ypos, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"colscalebar_height",(char *)&colscalebar_height,
                                                             TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"colscalebar_width",(char *)&colscalebar_width,
                                                             TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"colscalebar_xpos",(char *)&colscalebar_xpos,
                                                             TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"colscalebar_ypos",(char *)&colscalebar_ypos,
                                                             TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"sol_lat0",(char *)&sol_lat0,TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"sol_lat1",(char *)&sol_lat1,TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"sol_pthresh",(char *)&sol_pthresh,TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"sol_pslope",(char *)&sol_pslope,TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"sol_maxrat",(char *)&sol_maxrat,TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"sol_baseline_period",(char *)&sol_baseline_period,
                                                             TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"sol_baseline_end",(char *)&sol_baseline_end,
                                                             TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"sol_loflim",(char *)&sol_loflim,TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"sol_hiflim",(char *)&sol_hiflim,TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"sol_snr_rms",(char *)&sol_snr_rms,TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"phasecontour_min",(char *)&phasecontour_min,
                                                             TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"phasecontour_max",(char *)&phasecontour_max,
                                                             TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"dip_spacing",(char *)&dip_spacing,TCL_LINK_DOUBLE);

  /* begin rkt */
  Tcl_LinkVar(interp,"timeresolution",(char *)&func_time_resolution, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"fmin",(char *)&sclv_value_min, TCL_LINK_DOUBLE);
  Tcl_LinkVar(interp,"fmax",(char *)&sclv_value_max, TCL_LINK_DOUBLE);
  /* end rkt */

  /*=======================================================================*/
  /***** link global malloc'ed STRING vars */
  Tcl_LinkVar(interp,"spherereg",        (char *)&sphere_reg,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"home",        (char *)&subjectsdir,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"subject",     (char *)&pname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"hemi",        (char *)&stem,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"ext",         (char *)&ext,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"curv",        (char *)&cfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"sulc",        (char *)&kfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"patch",       (char *)&pfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"label",       (char *)&lfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"fs",          (char *)&fsfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"fm",          (char *)&fmfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"dip",         (char *)&dfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"dec",         (char *)&sfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"num_rgbdir",  (char *)&gfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"named_rgbdir",(char *)&sgfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"rgb",         (char *)&agfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"insurf",      (char *)&ifname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"outsurf",     (char *)&ofname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"targsurf",    (char *)&if2name,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"targcurv",    (char *)&cf2name,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"targcurvim",  (char *)&cif2name,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"val",         (char *)&vfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"annot",       (char *)&nfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"area",        (char *)&afname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"session",     (char *)&srname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"script",      (char *)&rfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"origcoords",  (char *)&orfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"paintcoords",  (char *)&paint_fname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"ellcoords",   (char *)&elfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"vrmlsurf",    (char *)&vrfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"subjtmpdir",  (char *)&tfname,TCL_LINK_STRING);
  Tcl_LinkVar(interp,"transform",   (char *)&xffname,TCL_LINK_STRING);

  /* begin rkt */
  Tcl_LinkVar(interp,"colortablename", 
        (char *)&labl_color_table_name,TCL_LINK_STRING);
  /* end rkt */

  /*=======================================================================*/

  strcpy(rfname,script_tcl);  /* save in global (malloc'ed in Surfer) */
  if (aliasflag) {
    sprintf(alias_tcl,"%s/lib/tcl/%s",envptr,argv[5]);
    Tcl_SetVar(interp,"aliasedscript",alias_tcl,0);
  }

  /* run tcl/tk startup script to set vars, make interface; no display yet */
  printf("tksurfer: interface: %s\n",tksurfer_tcl);
  code = Tcl_EvalFile(interp,tksurfer_tcl);
  if (*interp->result != 0 || code != TCL_OK)  
    printf(interp->result);

  /* begin rkt */
  /* this is ugly, but i have to to do this here because tcl is not inited
     when the func volume is first loaded. */
  if (func_timecourse!=NULL)
    {
      /* send the number of conditions */
      sprintf (tcl_cmd, "Graph_SetNumConditions %d", func_num_conditions);
      Tcl_Eval(interp,tcl_cmd);

      /* show the graph window */
      Tcl_Eval(interp,"Graph_ShowWindow");
    }
  if (func_timecourse_offset!=NULL)
    {
      /* turn on the offset options */
      Tcl_Eval(g_interp, "Graph_ShowOffsetOptions 1");
    }
  /* end rkt */

  /* begin rkt */

  /* disable certain menu sets */
  enable_menu_set (MENUSET_VSET_INFLATED_LOADED, 0);
  enable_menu_set (MENUSET_VSET_WHITE_LOADED, 0);
  enable_menu_set (MENUSET_VSET_PIAL_LOADED, 0);
  /*  enable_menu_set (MENUSET_VSET_ORIGINAL_LOADED, 0);*/
  if (NULL == func_timecourse)
    enable_menu_set (MENUSET_TIMECOURSE_LOADED, 0);
  enable_menu_set (MENUSET_OVERLAY_LOADED, 0);
  enable_menu_set (MENUSET_CURVATURE_LOADED, 0);
  enable_menu_set (MENUSET_LABEL_LOADED, 0);
  /* end rkt */

  /* if command line script exists, now run as batch job (possibly exiting) */
  if (scriptok) {    /* script may or may not open gl window */
    printf("tksurfer: run tcl script: %s\n",script_tcl);
    code = Tcl_EvalFile(interp,script_tcl);
    if (*interp->result != 0)  printf(interp->result);
  } else {
    ; /* surfer has already opened gl window if no script */
  } 

  /* always start up command line shell too (if script doesn't exit) */
  Tk_CreateFileHandler(0, TK_READABLE, StdinProc, (ClientData) 0);
  if (tty)
    Prompt(interp, 0);
  fflush(stdout);
  Tcl_DStringInit(&command);
  Tcl_ResetResult(interp);

  /*Tk_MainLoop();*/  /* standard */

  if (statflag)   /* uggh rkt and brf */
  {
    char cmd[STRLEN] ;

    statflag = 0 ;  /* I'm really, really sorry */
    sclv_set_current_field(SCLV_VALSTAT) ;
    sprintf (cmd, "ShowValueLabel %d 1", SCLV_VALSTAT);
    Tcl_Eval (g_interp, cmd);
    sprintf (cmd, "UpdateLinkedVarGroup view");
    Tcl_Eval (g_interp, cmd);
  }
  /* dual event loop (interface window made now) */
  while(tk_NumMainWindows > 0) {
    while (Tk_DoOneEvent(TK_ALL_EVENTS|TK_DONT_WAIT)) {
      /* do all the tk events; non-blocking */
    }
    do_one_gl_event(interp);

#if defined(Linux) || defined(sun) || defined(SunOS)
    tv.tv_sec = 0;
    tv.tv_usec = 10000;
    select(0, NULL, NULL, NULL, &tv);
#else
    sginap((long)1);   /* block for 10 msec */
#endif 

  }                                           

  Tcl_Eval(interp, "exit");
  exit(0);
  return(0) ;   /* for ansi */
}

/*=== from TkMain.c ===================================================*/
static void StdinProc(clientData, mask)
  ClientData clientData;
  int mask;
{
#define BUFFER_SIZE 4000
  char input[BUFFER_SIZE+1];
  static int gotPartial = 0;
  char *cmd;
  int code, count;

  count = read(fileno(stdin), input, BUFFER_SIZE);
  if (count <= 0) {
    if (!gotPartial) {
      if (tty) {Tcl_Eval(interp, "exit"); exit(1);}
      else     {Tk_DeleteFileHandler(0);}
      return;
    }
    else count = 0;
  }
  cmd = Tcl_DStringAppend(&command, input, count);
  if (count != 0) {
    if ((input[count-1] != '\n') && (input[count-1] != ';')) {
      gotPartial = 1;
      goto prompt; }
    if (!Tcl_CommandComplete(cmd)) {
      gotPartial = 1;
      goto prompt; }
  }
  gotPartial = 0;
  Tk_CreateFileHandler(0, 0, StdinProc, (ClientData) 0);
  code = Tcl_RecordAndEval(interp, cmd, TCL_EVAL_GLOBAL);
  Tk_CreateFileHandler(0, TK_READABLE, StdinProc, (ClientData) 0);
  Tcl_DStringFree(&command);
  if (*interp->result != 0)
    if ((code != TCL_OK) || (tty))
      puts(interp->result);
  prompt:
  if (tty)  Prompt(interp, gotPartial);
  Tcl_ResetResult(interp);
}

/*=== from TkMain.c ===================================================*/
static void Prompt(interp, partial)
  Tcl_Interp *interp;
  int partial;
{
  char *promptCmd;
  int code;

  promptCmd = Tcl_GetVar(interp,
      partial ? "tcl_prompt2" : "tcl_prompt1", TCL_GLOBAL_ONLY);
  if (promptCmd == NULL) {
    defaultPrompt:
    if (!partial)
      fputs("% ", stdout);
  }
  else {
    code = Tcl_Eval(interp, promptCmd);
    if (code != TCL_OK) {
      Tcl_AddErrorInfo(interp,
                    "\n    (script that generates prompt)");
      fprintf(stderr, "%s\n", interp->result);
      goto defaultPrompt;
    }
  }
  fflush(stdout);
}

static void
grabPixels(unsigned int width, unsigned int height, unsigned short *red, 
           unsigned short *green, unsigned short *blue)
{
  GLint    swapbytes, lsbfirst, rowlength ;
  GLint    skiprows, skippixels, alignment ;

  glGetIntegerv(GL_UNPACK_SWAP_BYTES, &swapbytes) ;
  glGetIntegerv(GL_UNPACK_LSB_FIRST, &lsbfirst) ;
  glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowlength) ;
  glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skiprows) ;
  glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skippixels) ;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment) ;

  glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE) ;
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0) ;
  glPixelStorei(GL_UNPACK_SKIP_ROWS, 0) ;
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0) ;
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1) ;

  glReadPixels(0, 0, width, height,GL_RED,GL_UNSIGNED_SHORT, (GLvoid *)red);
  glReadPixels(0, 0,width,height,GL_GREEN,GL_UNSIGNED_SHORT,(GLvoid *)green);
  glReadPixels(0, 0, width, height,GL_BLUE,GL_UNSIGNED_SHORT,(GLvoid *)blue);

  glPixelStorei(GL_UNPACK_SWAP_BYTES, swapbytes) ;
  glPixelStorei(GL_UNPACK_LSB_FIRST, lsbfirst) ;
  glPixelStorei(GL_UNPACK_ROW_LENGTH, rowlength) ;
  glPixelStorei(GL_UNPACK_SKIP_ROWS, skiprows) ;
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, skippixels) ;
  glPixelStorei(GL_UNPACK_ALIGNMENT, alignment) ;
}
static void
save_rgbfile(char *fname, int width, int height, unsigned short *red, 
         unsigned short *green, unsigned short *blue)
{
  RGB_IMAGE  *image ;
  int    y ;
  unsigned short *r, *g, *b ;

#ifdef IRIX
  image = iopen(fname,"w",RLE(1), 3, width, height, 3);
#else
  image = iopen(fname,"w",UNCOMPRESSED(1), 3, width, height, 3);
#endif
  if (!image)
    return ;
  for(y = 0 ; y < height; y++) 
  {
    r = red + y * width ;
    g = green + y * width ;
    b = blue + y * width ;

    /* fill rbuf, gbuf, and bbuf with pixel values */
    putrow(image, r, y, 0);    /* red row */
    putrow(image, g, y, 1);    /* green row */
    putrow(image, b, y, 2);    /* blue row */
  }
  iclose(image);
}

void
fix_nonzero_vals(void)
{
  int    vno ;
  VERTEX *v ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (!FZERO(v->val))
      v->fixedval = TRUE ;
    else
      v->fixedval = FALSE ;
  }
}

void
curv_to_val(void)
{
  int    vno ;
  VERTEX *v ;

  enable_menu_set (MENUSET_OVERLAY_LOADED, 1);
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    v->val = v->curv ;
  }
}
int
read_parcellation(char *parc_name, char *lut_name)
{
  return(read_and_smooth_parcellation(parc_name, lut_name, 25, 25)) ;
}
int
read_and_smooth_parcellation(char *parc_name, char *lut_name, 
                             int siter, int miter)
{
  char   *cp, fname[STRLEN], path[STRLEN], name[STRLEN] ;
  MRI    *mri ;
  int    vno, index, rd, gn, bl, xv, yv, zv ;
  FILE   *fp ;
  char   r[256], g[256], b[256], line[STRLEN] ;
  VERTEX *v ;
  Real   x, y, z ;

  MRISclearMarks(mris) ;
  cp = strchr(parc_name, '/') ;
  if (!cp)                 /* no path - use same one as mris was read from */
  {
    FileNamePath(mris->fname, path) ;
    sprintf(fname, "%s/../mri/%s", path, parc_name) ;
  }
  else   
    strcpy(fname, parc_name) ;  /* path specified explcitly */
  fprintf(stderr, "reading parcellation from %s...\n", fname) ;
  mri = MRIread(fname) ;
  if (!mri)
  {
    fprintf(stderr, "### could not read parcellation file %s\n", fname) ;
    return(Gerror) ;
  }

  cp = strchr(lut_name, '/') ;
  if (!cp)                 /* no path - use same one as mris was read from */
  {
    FileNamePath(mris->fname, path) ;
    sprintf(fname, "%s/../label/%s", path, lut_name) ;
  }
  else   
    strcpy(fname, lut_name) ;  /* path specified explcitly */

  fprintf(stderr, "reading color lut from %s...\n", fname) ;
  fp = fopen(fname, "r") ;
  if (!fp)
  {
    fprintf(stderr, "### could not read parcellation lut from %s\n", fname) ;
    return(Gerror) ;
  }


  while ((cp = fgetl(line, 199, fp)) != NULL)
  {
    sscanf(cp, "%d %s %d %d %d %*s\n", &index, name, &rd, &gn, &bl) ;
    r[index] = (char)rd ; g[index] = (char)gn ; b[index] = (char)bl ;
    parc_names[index] = (char *)calloc(strlen(name)+1, sizeof(char)) ;
    strcpy(parc_names[index], name) ;
    parc_red[index] = (char)rd ; parc_green[index] = (char)gn ; 
    parc_blue[index] = (char)bl ;
    parc_flag = 1 ;
  }
  fclose(fp) ;

  MRISsaveVertexPositions(mris, TMP_VERTICES) ;
  read_orig_vertex_coordinates("pial") ;
  MRISrestoreVertexPositions(mris, ORIGINAL_VERTICES) ;
  fprintf(stderr, "painting parcellation onto surface...\n") ;
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    MRISvertexToVoxel(v, mri, &x, &y, &z) ;
    xv = nint(x) ; yv = nint(y) ; zv = nint(z) ;
    xv = MAX(0,xv) ; yv = MAX(0,yv) ; zv = MAX(0,zv) ;
    xv = MIN(mri->width-1, xv) ;
    yv = MIN(mri->height-1, yv) ;
    zv = MIN(mri->depth-1, zv) ;
    index = MRIvox(mri, xv, yv, zv) ;
    if (index < 1)
      continue ;
    v->marked = 1 ;
    v->val = (float)index ; rd = r[index] ; gn = g[index] ; bl = b[index] ;
    v->annotation = rd + (gn << 8) + (bl << 16) ;
  }

  MRISsoapBubbleVals(mris, siter) ;
  fprintf(stderr, "applying mode filter...\n") ;
  MRISmodeFilterVals(mris, miter) ;
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    index = nint(v->val) ;
    rd = r[index] ; gn = g[index] ; bl = b[index] ;
    v->annotation = rd + (gn << 8) + (bl << 16) ;
  }
  MRISrestoreVertexPositions(mris, TMP_VERTICES) ;

  PR ;
  annotationloaded = TRUE ;
  MRIfree(&mri) ;
  MRISclearMarks(mris) ;
  return(NO_ERROR) ;
}
int
read_curv_to_val(char *fname)
{
  int    vno ;
  VERTEX *v ;

  enable_menu_set (MENUSET_OVERLAY_LOADED, 1);
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    v->tx = v->curv ;
  }
  surface_compiled = 0 ;
  if (MRISreadCurvatureFile(mris, fname) != NO_ERROR)
    return(Gerror) ;

  printf("surfer: values read: min=%f max=%f\n",
         mris->min_curv,mris->max_curv);
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    v->val = v->curv ;
    v->curv = v->tx ;
  }
  return(NO_ERROR) ;
}

void
val_to_curv(void)
{
  int    vno ;
  VERTEX *v ;

  enable_menu_set (MENUSET_OVERLAY_LOADED, 1);
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    v->curv = v->val ;
  }
}

void
val_to_stat(void)
{
  int    vno ;
  VERTEX *v ;

  enable_menu_set (MENUSET_OVERLAY_LOADED, 1);
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    v->stat = v->val ;
  }
}

void
stat_to_val(void)
{
  int    vno ;
  VERTEX *v ;

  enable_menu_set (MENUSET_OVERLAY_LOADED, 1);
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    v->val = v->stat ;
  }
}

void
show_flat_regions(char *surf_name, double thresh)
{
  int    vno, nfound = 0 ;
  VERTEX *v ;
  float  mx ;

  MRISsaveVertexPositions(mris, TMP_VERTICES) ;
  if (MRISreadVertexPositions(mris, surf_name) != NO_ERROR)
  {
    ErrorPrintf(ERROR_NOFILE, "show_flat_regions(%s): could not read surface",
                surf_name) ;
    return ;
  }

  MRIScomputeMetricProperties(mris) ;  /* compute normals */

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    mx = MAX(MAX(fabs(v->nx), fabs(v->ny)), fabs(v->nz)) ;
    if (mx > thresh)
    {
      nfound++ ;
      v->val = mx ;
    }
    else
      v->val = 0 ;
  }
  MRISrestoreVertexPositions(mris, TMP_VERTICES) ;
  MRIScomputeMetricProperties(mris) ;
  printf("%d vertices marked...\n", nfound) ;
  enable_menu_set (MENUSET_OVERLAY_LOADED, 1);
}

void
val_to_mark(void)
{
  int    vno ;
  VERTEX *v ;
  static int mark = 2 ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    if (fabs(v->val) > fthresh && v->marked < 2)
      v->marked = mark ;
  }
  mark++ ;
}

void
twocond(int c0, int c1)
{
  char  fname[STRLEN], val_name[STRLEN], *cp ;
  int   dof ;
  FILE  *fp ;

  cond0 = c0 ; cond1 = c1 ;

  /* put the averages in val and val2, and the variances in valbak, val2bak */
  twocond_flag = 1 ;
  sprintf(fname, "%s/sigavg%d-%s.w", val_dir, c1, stem) ;
  fprintf(stderr, "reading cond %d means from %s\n", c1, fname) ;
  surface_compiled = 0 ;
  MRISreadValues(mris, fname) ;
  MRIScopyValToVal2(mris) ;

  sprintf(fname, "%s/sigavg%d.dof", val_dir, c1) ;
  fp = fopen(fname, "r") ;
  if (!fp)
  {
    printf("### surfer: could not open dof file %s\n", fname) ;
    return ;
  }
  fscanf(fp, "%d", &dof) ; fclose(fp) ;
  sprintf(fname, "%s/sigvar%d-%s.w", val_dir, c1, stem) ;
  fprintf(stderr, "reading cond %d variances from %s and scaling by %d\n", 
          c1, fname, dof) ;
  MRISreadValues(mris, fname) ;
  /* turn squared standard errors into variances */
  MRISmulVal(mris, (float)dof) ; MRISsqrtVal(mris) ;
  MRIScopyValToVal2Bak(mris) ;

  sprintf(fname, "%s/sigavg%d.dof", val_dir, c0) ;
  fp = fopen(fname, "r") ;
  if (!fp)
  {
    printf("### surfer: could not open dof file %s\n", fname) ;
    return ;
  }
  fscanf(fp, "%d", &dof) ; fclose(fp) ;
  sprintf(fname, "%s/sigvar%d-%s.w", val_dir, c0, stem) ;
  fprintf(stderr, "reading cond %d variances from %s and scaling by %d\n", 
          c0, fname,dof) ;
  MRISreadValues(mris, fname) ;
  /* turn squared standard errors into variances */
  MRISmulVal(mris, (float)dof) ; MRISsqrtVal(mris) ;
  MRIScopyValToValBak(mris) ;

  sprintf(fname, "%s/sigavg%d-%s.w", val_dir, c0, stem) ;
  fprintf(stderr, "reading condition %d means from %s\n", 
          c0, fname) ;
  MRISreadValues(mris, fname) ;
  sprintf(val_name, "GROUP%d", c0) ;
  cp = getenv(val_name) ;
  if (cp)
    sprintf(val_name, "%s mean", cp) ;
  else
    sprintf(val_name, "group %d mean", c0) ;
  set_value_label_name(val_name, SCLV_VAL) ;
  if (cp)
    sprintf(val_name, "%s std", cp) ;
  else
    sprintf(val_name, "group %d std", c0) ;
  set_value_label_name(val_name, SCLV_VALBAK) ;
  sprintf(val_name, "GROUP%d", c1) ;
  cp = getenv(val_name) ;
  if (cp)
    sprintf(val_name, "%s mean", cp) ;
  else
    sprintf(val_name, "group %d mean", c1) ;
  set_value_label_name(val_name, SCLV_VAL2) ;
  if (cp)
    sprintf(val_name, "%s std", cp) ;
  else
    sprintf(val_name, "group %d std", c1) ;
  set_value_label_name(val_name, SCLV_VAL2BAK) ;
}

int
mask_label(char *label_name)
{
  LABEL  *area ;
  int     vno ;
  VERTEX  *v ;

  surface_compiled = 0 ;
  MRISclearMarks(mris) ;
  area = LabelRead(pname, label_name) ;
  if (!area)
  {
    fprintf(stderr, "unable to read label file %s\n", label_name) ;
    return(NO_ERROR) ;
  }

  LabelMarkSurface(area, mris) ;  /* mark all points in label */

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->marked || v->ripflag)
      continue ;
    v->stat = v->val = v->imag_val = v->val2 = v->valbak = v->val2bak = 0.0 ;
  }
  LabelFree(&area) ;
  MRISclearMarks(mris) ;
  redraw() ;
  return(NO_ERROR) ;
}

#include "mrishash.h"
static void
deconvolve_weights(char *weight_fname, char *scale_fname)
{
  MHT     *mht ;
  MHBT    *bucket ;
  MHB     *bin ;
  int     vno, n, i ;
  VERTEX  *v, *vn ;
  double  angle, circumference, norm, sigma_sq, wt, mean, var, mn, mx,
          sphere_area ;
  float   radius, dist, sigma, x0, y0, z0, dscale, dx, dy, dz ;
  VECTOR  *v1, *v2 ;


  surface_compiled = 0 ;
  if (is_val_file(weight_fname))
  {
    if (MRISreadValues(mris,weight_fname) != NO_ERROR)
      return ;
  }
  else if (read_curv_to_val(weight_fname) != NO_ERROR)
    return ;
    
  fprintf(stderr, "loading spherical coordinate system...\n") ;
  if (read_canon_vertex_coordinates("sphere") != NO_ERROR)
    return ;
  
  MRISsaveVertexPositions(mris, TMP_VERTICES) ;
  MRISrestoreVertexPositions(mris, CANONICAL_VERTICES) ;
  radius = MRISaverageRadius(mris) ;
  sphere_area = M_PI * radius * radius * 4.0 ;
  dscale = sqrt(mris->total_area / sphere_area) ;
  circumference = M_PI * 2.0 * radius ;
  v1 = VectorAlloc(3, MATRIX_REAL) ;
  v2 = VectorAlloc(3, MATRIX_REAL) ;
  MRISrestoreVertexPositions(mris, TMP_VERTICES) ;
  fprintf(stderr, "average radius = %2.1f\n", radius) ;

  MRIScopyValToVal2(mris) ;   /* put weights in val2 field */
  if (MRISreadValues(mris, scale_fname) != NO_ERROR)
  {
    fprintf(stderr, "### surfer: could not open scale file %s\n",scale_fname); 
    PR ;
    return ;
  }
  MRIScopyValToVal2Bak(mris) ;   /* put spatial scale in val2bak field */

#define MAX_DIST  (1.0*sqrt(500))

  fprintf(stderr, "building spatial LUT...\n") ;
  mht = MHTfillVertexTableRes(mris, NULL, CANONICAL_VERTICES, MAX_DIST) ;

  fprintf(stderr, "deconvolving weights...\n") ;
  MRISsetVals(mris, 0.0) ;  /* clear all values */

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    v->tdx = 0 ;  /* used for normalization below */
  }
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    if (!(vno % (mris->nvertices/50)))
      fprintf(stderr, ".") ;
    v = &mris->vertices[vno] ;
    if (vno == Gdiag_no)
      DiagBreak() ;
    if (v->ripflag || (fabs(v->val2) < fthresh))
      continue ;

    x0 = v->cx ; y0 = v->cy ; z0 = v->cz ;
    sigma = sqrt(v->val2bak) ;  
    if (FZERO(sigma))
      norm = 1.0 ;
    else
      norm = 1.0 / (sigma * sqrt(2.0*M_PI)) ;
    sigma_sq = sigma*sigma ;
    MRISclearMarks(mris) ;

    VECTOR_LOAD(v1, x0, y0, z0) ;  /* radius vector */

    for (dx = -MAX_DIST ; dx <= MAX_DIST ; dx += MAX_DIST)
    for (dy = -MAX_DIST ; dy <= MAX_DIST ; dy += MAX_DIST)
    for (dz = -MAX_DIST ; dz <= MAX_DIST ; dz += MAX_DIST)
    {
      x0 = v->cx + dx ; y0 = v->cy + dy ; z0 = v->cz + dz ; 
      
      bucket = MHTgetBucket(mht, x0, y0, z0) ;
      if (!bucket)
        continue ;
      for (bin = bucket->bins, i = 0 ; i < bucket->nused ; i++, bin++)
      {
        n = bin->fno ; vn = &mris->vertices[n] ;
        if (vn->marked)
          continue ;
        if (n == Gdiag_no)
          DiagBreak() ;
        vn->marked = 1 ;  /* only process it once */
        VECTOR_LOAD(v2, vn->cx, vn->cy, vn->cz) ;  /* radius vector */
        angle = fabs(Vector3Angle(v1, v2)) ;
        dist = dscale * circumference * angle / (2.0 * M_PI) ;
        if (dist > 3*sigma)
          continue ;
        if (n == Gdiag_no)
          DiagBreak() ;
        if (FZERO(sigma_sq)) /* make it a unit delta function */
        {
          if (FZERO(dist))
            wt = norm ;
          else
            wt = 0.0 ;
        }
        else
          wt = norm*exp(-0.5 * dist*dist / sigma_sq) ;
#if 0
        if (wt < fthresh/10)
          continue ;
#endif
        if (n == Gdiag_no)
          DiagBreak() ;
        vn->val += v->val2*wt ;
        vn->tdx += wt ;            /* for normalization later */
        if (!finite(vn->val))
          DiagBreak() ;
      }
    }
  }
  fprintf(stderr, "\n") ;

  mean = var = mn = mx = 0.0f ;
  for (i = vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (vno == Gdiag_no)
      DiagBreak() ;
    if (!FZERO(v->tdx))
    {
      i++ ;
      v->val /= v->tdx ;
      v->tdx = 0 ;
      if (v->val < mn)
        mn = v->val ;
      if (v->val > mx)
        mx = v->val ;
      mean += v->val ;
      var += v->val * v->val ;
    }
  }

  if (i)
  {
    mean /= (double)i ;
    var = var / i - mean*mean ;
    fprintf(stderr, "%d non-zero vals, [%2.2f --> %2.2f], %2.2f += %2.2f\n",
            i, mn, mx, mean, sqrt(var)) ;
  }
  overlayflag = TRUE ;
  colscale = HEAT_SCALE ;
  val_to_stat() ;
  complexvalflag = FALSE ;

  MRISclearMarks(mris) ;
  VectorFree(&v1) ; VectorFree(&v2) ;
  MHTfree(&mht) ;
  redraw() ;
}

static void
read_disc(char *subject_name)
{
  char   fname[300] ;
  int    vno ;
  VERTEX *v ;
  double proj ;
  
  surface_compiled = 0 ;
  sprintf(fname, "./%s.offset_%s", stem, subject_name) ;
  if (read_curv_to_val(fname) != NO_ERROR)
  {
    fprintf(stderr, "### surfer: could not open %s\n", fname) ;
    return ;
  }
  shift_values() ;
  sprintf(fname, "./%s.mdiff", stem) ;
  if (read_curv_to_val(fname) != NO_ERROR)
  {
    fprintf(stderr, "### surfer: could not open %s\n", fname) ;
    return ;
  }

  swap_values() ;  /* mdiff ->valbak, offset ->val2bak */

  sprintf(fname, "control_thickness/%s.thickness_%s", stem, subject_name) ;
  if (read_curv_to_val(fname) != NO_ERROR)
  {
    fprintf(stderr, "### surfer: could not open %s\n", fname) ;
    return ;
  }
  shift_values() ;    /* thickness -> val2 */
  sprintf(fname, "./%s.disc_%s", stem, subject_name) ; /* disc -> val */
  if (read_curv_to_val(fname) != NO_ERROR)
  {
    fprintf(stderr, "### surfer: could not open %s\n", fname) ;
    return ;
  }

  MRIScopyValuesToImagValues(mris) ;   /* disc -> imag_val */

  /*
    valbak       - difference between means
    val2bak      - offset (mean of two groups)
    val2         - thickness of individual
    imag_val     - discriminant weight for subject
    val          - projection of mean difference onto discriminant
  */
    
  disc_flag = 1 ;
  mask_label("lh-cortex") ;
  for (proj = 0.0, vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    v->val = (v->val2 - v->val2bak) * v->imag_val ;
    proj += (double)v->val ;
  }
  printf("projection onto discriminant: %2.3f\n", proj) ;
  PR 
}

/* begin rkt */

static void
print_vertex_data(int vno, FILE *fp, float dmin)
{
  VERTEX   *v ;
  int      i, j, imnr ;

  v = &mris->vertices[vno] ;
  if (twocond_flag)
  {
    if (!FZERO(v->imag_val))
      fprintf(fp, "cond %d: %2.3f +- %2.3f, cond %d: %2.3f +- %2.3f, "
              "scale=%2.0f\n",
              cond0, v->val, v->valbak, cond1, v->val2, v->val2bak,
              v->imag_val);
    else
      fprintf(fp, "cond %d: %2.3f +- %2.3f, cond %d: %2.3f +- %2.3f\n",
              cond0, v->val, v->valbak, cond1, v->val2, v->val2bak);
    PR;
  }
  else if (disc_flag)
  {
    fprintf(fp, "v %d:\n\tdisc:\t\t%2.3f\n\tmdiff:"
            "\t\t%2.3f\n\tthickness:\t%2.3f\n\toffset:\t\t%2.3f\n"
            "\tdiff:\t\t%2.3f\n\tproj:\t\t%2.3f\n",
            vno, v->imag_val, v->valbak, v->val2, v->val2bak,
            v->val2-v->val2bak, v->val); PR;
    
  }
  else
  {
#if 0
    fprintf(fp, "surfer: dmin=%3.4f, vno=%d, x=%3.4f, y=%3.4f, z=%3.4f, "
           "nz=%3.4f\n", dmin,vno,v->x,
           v->y,
           v->z,nzs);PR;
#else
    fprintf(fp, "surfer: dmin=%3.4f, vno=%d, x=%3.4f, y=%3.4f, z=%3.4f\n", 
           dmin,vno,v->x, v->y, v->z);PR;
#endif
    fprintf(fp, "surfer: curv=%f, fs=%f\n",v->curv,v->fieldsign);PR;
    fprintf(fp, "surfer: val=%f, val2=%f\n",v->val,v->val2);PR;
    fprintf(fp, "surfer: amp=%f, angle=%f deg (%f)\n",hypot(v->val,v->val2),
           (float)(atan2(v->val2,v->val)*180/M_PI),
           (float)(atan2(v->val2,v->val)/(2*M_PI)));PR;
  }
  if (annotationloaded)
  {
    int  r, g, b ;
    r = v->annotation & 0x00ff ;
    g = (v->annotation >> 8) & 0x00ff ;
    b = (v->annotation >> 16) & 0x00ff ;
    fprintf(fp, "annot = %d (%d, %d, %d)\n", v->annotation, r, g, b) ;
  }
  if (MRIflag && MRIloaded)
  {
    imnr = (int)((v->y-yy0)/st+0.5);
    i = (int)((zz1-v->z)/ps+0.5);
    j = (int)((xx1-v->x)/ps+0.5);
    fprintf(fp, "surfer: mrival=%d imnr=%d, i=%d, j=%d\n",
           im[imnr][i][j],imnr,i,j);PR;
  }
  if (parc_flag && v->val > 0 && parc_names[(int)nint(v->val)])
    fprintf(stderr, "parcellation name: %s\n", parc_names[(int)nint(v->val)]) ;
}

static void 
update_labels(int label_set, int vno, float dmin)
{
  int err;
  char command[NAME_LENGTH];
  VERTEX *v;
  int i, j, imnr;
  int r, g, b;
  float x_mni, y_mni, z_mni;
  float x_tal, y_tal, z_tal;
  float value;
  char fname[NAME_LENGTH];
  float x, y, z, sx, sy, sz, rr, dd, phi, theta;
  int field;
  int label_index;

  /* make sure we an interpreter to send commands to */
  if(g_interp==NULL)
    return;

  /* get the vertex */
  v = &mris->vertices[vno];

  /* send each label value */
  sprintf(command, "UpdateLabel %d %d %d", label_set, LABEL_VERTEXINDEX, vno);
  Tcl_Eval(g_interp, command);
  sprintf(command, "UpdateLabel %d %d %f", label_set, LABEL_DISTANCE, dmin);
  Tcl_Eval(g_interp, command);
  sprintf(command, "UpdateLabel %d %d \"(%.2f, %.2f, %.2f)\"", 
    label_set, LABEL_COORDS_RAS, v->origx, v->origy, v->origz);
  Tcl_Eval(g_interp, command);
  if (MRIflag && MRIloaded) 
    {
      imnr = (int)((v->y-yy0)/st+0.5);
      i = (int)((zz1-v->z)/ps+0.5);
      j = (int)((xx1-v->x)/ps+0.5);
      sprintf(command, "UpdateLabel %d %d \"(%d, %d, %d)\"",
        label_set, LABEL_COORDS_INDEX, imnr, i, j);
      Tcl_Eval(g_interp, command);
      sprintf(command, "UpdateLabel %d %d %d", 
        label_set, LABEL_MRIVALUE, im[imnr][i][j]);
      Tcl_Eval(g_interp, command);
    }
  /* if we have a tal transform, compute the tal. */
  if (transform_loaded)
    {
      LTAworldToWorld(lta, v->origx, v->origy, v->origz, 
          &x_mni, &y_mni, &z_mni) ;
      sprintf(command, "UpdateLabel %d %d \"(%.2f, %.2f, %.2f)\"", 
        label_set, LABEL_COORDS_MNITAL, x_mni, y_mni, z_mni );
      Tcl_Eval(g_interp, command);

      /* now conver mni tal to the real tal. */
      conv_mnital_to_tal( x_mni, y_mni, z_mni, &x_tal, &y_tal, &z_tal );
      sprintf(command, "UpdateLabel %d %d \"(%.2f, %.2f, %.2f)\"", 
        label_set, LABEL_COORDS_TAL, x_tal, y_tal, z_tal );
      Tcl_Eval(g_interp, command);
    }
  sprintf(command, "UpdateLabel %d %d \"(%.2f, %.2f, %.2f)\"", 
    label_set, LABEL_COORDS_NORMAL, v->nx, v->ny, v->nz);
  Tcl_Eval(g_interp, command);

  /* if a canon surface isn't loaded, make a name and see if it exists. */
  if (canonsurfloaded == FALSE && canonsurffailed == FALSE)
  {
    sprintf(fname, "%s.sphere.reg", fpref) ;
    if (FileExists(fname))
    {
      printf("surfer: reading canonical coordinates from\n");
      printf("surfer:   %s\n",fname);
    } else {
      /* set our don't load flag here so we don't try and load it every time */
      dontloadspherereg = TRUE;
    }
  }
  if (canonsurffailed)
      dontloadspherereg = TRUE;
    
  /* if the canon surface is loaded _or_ if the filename made above exists
     and we can read in the vertex coords and our don't load flag isn't
     on... */
  if (canonsurfloaded == TRUE || 
      (dontloadspherereg == FALSE && 
       FileExists(fname) && 
       read_canon_vertex_coordinates(fname) == NO_ERROR) )
  {
    x = mris->vertices[vno].cx ; 
    y = mris->vertices[vno].cy ; 
    z = mris->vertices[vno].cz ;
    sx = x*e0[0] + y*e0[1] + z*e0[2] ; 
    sy = x*e1[0] + y*e1[1] + z*e1[2] ; 
    sz = x*e2[0] + y*e2[1] + z*e2[2] ; 
    x = sx ; y = sy ; z = sz ;

    rr = sqrt(x*x + y*y + z*z) ;
    dd = rr*rr-z*z ; 
    if (dd < 0.0) 
      dd = 0.0 ;

    phi = atan2(sqrt(dd), z) ;
    theta = atan2(y/rr, x/rr) ;

    sprintf(command, "UpdateLabel %d %d \"(%.2f, %.2f, %.2f)\"",
      label_set, LABEL_COORDS_SPHERE_XYZ, sx, sy, sz );
    Tcl_Eval(g_interp, command);
    sprintf(command, "UpdateLabel %d %d \"(%2.1f, %2.1f)\"", 
      label_set, LABEL_COORDS_SPHERE_RT, DEGREES(phi), DEGREES(theta) );
    Tcl_Eval(g_interp, command);
  }

  sprintf(command, "UpdateLabel %d %d %f", 
    label_set, LABEL_CURVATURE, v->curv);
  Tcl_Eval(g_interp, command);
  sprintf(command, "UpdateLabel %d %d %f", 
    label_set, LABEL_FIELDSIGN, v->fieldsign);
  Tcl_Eval(g_interp, command);

  /* overlay labels. draw the current one in stars. */
  for (field = 0; field < NUM_SCALAR_VALUES; field++ ) 
  {
    sclv_get_value(v,field,&value);
    if (field == sclv_current_field)
    {
      sprintf(command, "UpdateLabel %d %d \"** %f **\"", label_set, 
              LABEL_VAL + field, value);
    } else {
      sprintf(command, "UpdateLabel %d %d %f", label_set, 
              LABEL_VAL + field, value);
    }
    Tcl_Eval(g_interp, command);
  }
#if 0
  sprintf(command, "UpdateLabel %d %d %f", label_set, LABEL_VAL, v->val);
  Tcl_Eval(g_interp, command);
  sprintf(command, "UpdateLabel %d %d %f", label_set, LABEL_VAL2, v->val2);
  Tcl_Eval(g_interp, command);
  sprintf(command, "UpdateLabel %d %d %f", label_set, LABEL_VALBAK, v->valbak);
  Tcl_Eval(g_interp, command);
  sprintf(command, "UpdateLabel %d %d %f", label_set, LABEL_VAL2BAK, v->val2bak);
  Tcl_Eval(g_interp, command);
  sprintf(command, "UpdateLabel %d %d %f", label_set, LABEL_VALSTAT, v->stat);
#endif

  Tcl_Eval(g_interp, command);
  sprintf(command, "UpdateLabel %d %d %f", label_set, LABEL_AMPLITUDE, 
    hypot(v->val,v->val2));
  Tcl_Eval(g_interp, command);
  sprintf(command, "UpdateLabel %d %d %f", label_set, LABEL_ANGLE, 
    (float)(atan2(v->val2,v->val)*180.0/M_PI));
  Tcl_Eval(g_interp, command);
  sprintf(command, "UpdateLabel %d %d %f", label_set, LABEL_DEGREE, 
    (float)(atan2(v->val2,v->val)/(2*M_PI)));
  Tcl_Eval(g_interp, command);

  /* send label update. */
  err = labl_find_label_by_vno( vno, &label_index );
  if (err == ERROR_NONE &&
      ( label_index >= 0 && label_index < labl_num_labels ))
    {
      sprintf(command, "UpdateLabel %d %d \"%s\"", label_set, LABEL_LABEL, 
        labl_labels[label_index].name );
      Tcl_Eval(g_interp, command);
    }
  else
    {
      sprintf(command, "UpdateLabel %d %d \"None.\"", label_set, LABEL_LABEL );
      Tcl_Eval(g_interp, command);
    }

  //  if (annotationloaded)
  //    {
      r = v->annotation & 0x00ff ;
      g = (v->annotation >> 8) & 0x00ff ;
      b = (v->annotation >> 16) & 0x00ff ;
      sprintf(command, "UpdateLabel %d %d \"%d (%d, %d, %d)\"", 
        label_set, LABEL_ANNOTATION, v->annotation, r, g, b);
      Tcl_Eval(g_interp, command);
      //    }

  if (parc_flag && v->val > 0 && parc_names[(int)nint(v->val)])
    {
      sprintf(command, "UpdateLabel %d %d \"%s\"", 
        label_set, LABEL_PARCELLATION_NAME, 
        parc_names[(int)nint(v->val)]);
      Tcl_Eval(g_interp, command);
    }
}

/* -------------------------------------------------- ctrl-c cancel support */

void cncl_initialize ()
{
  /* init the flags and register our handler. */
  cncl_listening = 0;
  cncl_canceled = 0;
  /* this doesn't seem to work. */
  //  signal (SIGINT, cncl_handle_sigint);
}

void cncl_start_listening ()
{
  /* set our listening flag. */
  cncl_listening = 1;
}

void cncl_stop_listening ()
{
  /* stop listening and reset the canceled flag. */
  cncl_listening = 0;
  cncl_canceled = 0;
}

int cncl_user_canceled () 
{
  /* just return the canceled flag. */
  return cncl_canceled;
}

void cncl_handle_sigint (int signal)
{
  /* if we're listening, set the flag, if not, exit normally. */
  if (cncl_listening) 
    {
      printf ("canceled!\n" );
      cncl_canceled = 1;
    } 
  else
    {
      printf ("Killed\n");
      exit (1);
    }
}

/* ------------------------------------------------------------------------ */

/* ------------------------------------------------------- menu set support */

int
enable_menu_set (int set, int enable) {
  char tcl_cmd[1024];

  if (g_interp)
    {
      strncpy (tcl_cmd, "tkm_SetMenuItemGroupStatus", sizeof(tcl_cmd));
      switch(set)
  {
  case MENUSET_VSET_INFLATED_LOADED:
    sprintf (tcl_cmd, "%s mg_InflatedVSetLoaded", tcl_cmd);
    break;
  case MENUSET_VSET_WHITE_LOADED:
    sprintf (tcl_cmd, "%s mg_WhiteVSetLoaded", tcl_cmd);
    break;
  case MENUSET_VSET_PIAL_LOADED:
    sprintf (tcl_cmd, "%s mg_PialVSetLoaded", tcl_cmd);
    break;
  case MENUSET_VSET_ORIGINAL_LOADED:
    sprintf (tcl_cmd, "%s mg_OriginalVSetLoaded", tcl_cmd);
    break;
  case MENUSET_TIMECOURSE_LOADED:
    sprintf (tcl_cmd, "%s mg_TimeCourseLoaded", tcl_cmd);
    break;
  case MENUSET_OVERLAY_LOADED:
    sprintf (tcl_cmd, "%s mg_OverlayLoaded", tcl_cmd);
    break;
  case MENUSET_CURVATURE_LOADED:
    sprintf (tcl_cmd, "%s mg_CurvatureLoaded", tcl_cmd);
    break;
  case MENUSET_LABEL_LOADED:
    sprintf (tcl_cmd, "%s mg_LabelLoaded", tcl_cmd);
    break;
  default:
    ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM,
             "enable_menu_set: bad set %d\n",set));
  }
      sprintf (tcl_cmd, "%s %d", tcl_cmd, enable);
      Tcl_Eval(g_interp, tcl_cmd);
      if (*g_interp->result != 0) printf(g_interp->result);
    }
  return(ERROR_NONE);
}

/* ------------------------------------------------------------------------ */

/* -------------------------------------------- multiple vertex set support */

int
vset_initialize()
{
  int i;
  /* set them all to null */
  for (i=0;i<NUM_VERTEX_SETS;i++)
      vset_vertex_list[i]=NULL;
  return(NO_ERROR);
}

int
vset_read_vertex_set(int set, char* fname)
{
  /* copy the current main verts into tmp */
  MRISsaveVertexPositions( mris, TMP_VERTICES );

  /* read the file */
  if( MRISreadVertexPositions( mris, fname ) != NO_ERROR ) {

    /* restore the vertices */
    MRISrestoreVertexPositions( mris, TMP_VERTICES );
    printf("surfer: couldn't load %s!\n", fname);
    ErrorReturn(ERROR_NOFILE,(ERROR_NOFILE,"vset_read_vertex_set: MRISreadVertexPositions failed\n"));
  }

  printf("surfer: loaded %s into set %d\n", fname, set);
 
  /* save the verts into external storage */
  vset_save_surface_vertices(set);

  /* copy the saved verts back into main space */
  MRISrestoreVertexPositions( mris, TMP_VERTICES );

  /* enable the set menu */
  switch(set)
    {
    case VSET_INFLATED: enable_menu_set (MENUSET_VSET_INFLATED_LOADED,1); break;
    case VSET_WHITE: enable_menu_set (MENUSET_VSET_WHITE_LOADED,1); break;
    case VSET_PIAL: enable_menu_set (MENUSET_VSET_PIAL_LOADED,1); break;
    case VSET_ORIGINAL: enable_menu_set (MENUSET_VSET_ORIGINAL_LOADED,1); break;
    default:
      break;
    }

  return(NO_ERROR);
}

int
vset_save_surface_vertices(int set) {

  int vno,nvertices;
  VERTEX *v;

  if (set < 0 || set > NUM_VERTEX_SETS)
    ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM,"vset_load_surface_vertices: invalid set %d\n",set));
  
  /* allocate storage if not done so already */
  if (vset_vertex_list[set]==NULL)
    {
      vset_vertex_list[set]=(VSET_VERTEX*) 
  calloc(mris->nvertices,sizeof(VSET_VERTEX));
      if (vset_vertex_list[set]==NULL)
  ErrorReturn(ERROR_NOMEMORY,(ERROR_NOMEMORY,"vset_save_surface_vertices: allocation of vset_vertex_list[%d] failed (size=%d)\n",set,mris->nvertices));
    }

  /* save all vertex values into storage */
  nvertices=mris->nvertices;
  for (vno=0;vno<nvertices;vno++)
    {
      v = &mris->vertices[vno];
      vset_vertex_list[set][vno].x = v->x;
      vset_vertex_list[set][vno].y = v->y;
      vset_vertex_list[set][vno].z = v->z;
    }

  return(NO_ERROR);
}

int
vset_load_surface_vertices(int set)
{

  int vno,nvertices;
  VERTEX* v;

  if (set < 0 || set > NUM_VERTEX_SETS)
    ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM,"vset_load_surface_vertices: invalid set %d\n",set));
  
  /* if not allocated, no verts stored there */
  if (vset_vertex_list[set]==NULL)
    {
      printf("surfer: vertex set not loaded.\n");
      return(NO_ERROR);
    }

  /* load all vertex values from storage */
  nvertices=mris->nvertices;
  for (vno=0;vno<mris->nvertices;vno++)
    {
      v = &mris->vertices[vno];
      v->x = vset_vertex_list[set][vno].x;
      v->y = vset_vertex_list[set][vno].y;
      v->z = vset_vertex_list[set][vno].z;
    }

  return(NO_ERROR);
}

int
vset_set_current_set(int set)
{
  if (set < 0 || set > NUM_VERTEX_SETS)
    ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM,"vset_load_surface_vertices: invalid set %d\n",set));

  /* save the main set if we haven't done so yet */
  if (vset_current_set==VSET_MAIN && vset_vertex_list[VSET_MAIN]==NULL)
    vset_save_surface_vertices(VSET_MAIN);

  /* load in over the main set */
  vset_load_surface_vertices(set);

  /* save the new vertex set. recompute the normals and mark our
     vertex array dirty so it will rebuild on next redraw. */
  vset_current_set = set;
  vertex_array_dirty = 1;
  compute_normals();
  return(NO_ERROR);
}

/* ------------------------------------------------------------------------ */

/* -------------------------------------------------- coordinate conversion */

int conv_initialize()
{

  /* allocate our conversion matrices. */
  if (NULL != conv_mnital_to_tal_m_ltz)
    MatrixFree (&conv_mnital_to_tal_m_ltz);

  conv_mnital_to_tal_m_ltz = MatrixIdentity (4, NULL);
  stuff_four_by_four (conv_mnital_to_tal_m_ltz,
          0.99,       0,     0, 0,
          0.00,  0.9688, 0.042, 0,
          0.00, -0.0485, 0.839, 0,
          0.00,       0,     0, 1);

  if (NULL != conv_mnital_to_tal_m_gtz)
    MatrixFree (&conv_mnital_to_tal_m_gtz);

  conv_mnital_to_tal_m_gtz = MatrixIdentity (4, NULL);
  stuff_four_by_four (conv_mnital_to_tal_m_gtz,
          0.99,       0,      0, 0,
          0.00,  0.9688,  0.046, 0,
          0.00, -0.0485, 0.9189, 0,
          0.00,       0,      0, 1);

  /* allocate our temporary matrices. */
  if (NULL != conv_tmp1_m)
    MatrixFree (&conv_tmp1_m);
  conv_tmp1_m = MatrixAlloc (4, 1, MATRIX_REAL);

  if (NULL != conv_tmp2_m)
    MatrixFree (&conv_tmp2_m);
  conv_tmp2_m = MatrixAlloc (4, 1, MATRIX_REAL);

  return(NO_ERROR);
}

int conv_mnital_to_tal(float mnix, float mniy, float mniz,
           float* talx, float* taly, float* talz) 
{
  *MATRIX_RELT(conv_tmp1_m,1,1) = mnix;
  *MATRIX_RELT(conv_tmp1_m,2,1) = mniy;
  *MATRIX_RELT(conv_tmp1_m,3,1) = mniz;
  *MATRIX_RELT(conv_tmp1_m,4,1) = 1.0;
  
  if (mniz > 0)
    {
      MatrixMultiply (conv_mnital_to_tal_m_gtz, conv_tmp1_m, conv_tmp2_m);
    }
  else
    {
      MatrixMultiply (conv_mnital_to_tal_m_ltz, conv_tmp1_m, conv_tmp2_m);
    }

  *talx = *MATRIX_RELT(conv_tmp2_m,1,1);
  *taly = *MATRIX_RELT(conv_tmp2_m,2,1);
  *talz = *MATRIX_RELT(conv_tmp2_m,3,1);

  return(NO_ERROR);
}

/* ------------------------------------------------------------------------ */

/* ----------------------------------------------------------- undo support */

int undo_initialize()
{
  int undo_index;
  for (undo_index = UNDO_LIST_POS_FIRST;
       undo_index <= UNDO_LIST_POS_LAST;
       undo_index++ )
    undo_list[undo_index] = NULL;

  undo_send_first_action_name();

  return(NO_ERROR);
}

int undo_get_action_node_size(int action_type) 
{
  int size=-1;

  /* return the size of the specific action node impelementation, or
     -1 as an error condition. */
  switch(action_type)
    {
    case UNDO_CUT: size = sizeof(UNDO_CUT_NODE); break;
    default: size = -1; break;
    }

  return(size);
}

char* undo_get_action_string(int action_type)
{
  char* string=NULL;

  /* if the action type is in bounds, return a string from the
     undo_action_strings array, else return a null string. */
  if (action_type >= UNDO_INVALID && action_type < NUM_UNDO_ACTIONS)
    string=undo_action_strings[action_type];
  else
    string=NULL;
  
  return(string);
}

int undo_begin_action(int action_type) 
{
  UNDO_ACTION* action = NULL;
  xGArr_tErr array_error = xGArr_tErr_NoErr;
  int action_size = 0;
  int undo_index = 0;

  if (undo_list_state!=UNDO_LIST_STATE_CLOSED)
    ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM,"undo_begin_action: undo list state is not closed\n"));

  if (action_type<=UNDO_NONE || action_type>=NUM_UNDO_ACTIONS)
    ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM,"undo_begin_action: invalid action type\n"));

  /* create a new undo action */
  action = (UNDO_ACTION*)malloc(sizeof(UNDO_ACTION));
  if (action==NULL)
    ErrorReturn(ERROR_NO_MEMORY,(ERROR_NO_MEMORY,"undo_begin_action: malloc UNDO_ACTION failed\n"));
  action->undo_type = action_type;
  
  /* get the action node size and allocate the node list */
  action_size = undo_get_action_node_size(action_type);
  if (action_size<=0)
    ErrorReturn(ERROR_UNDO_ACTION,(ERROR_UNDO_ACTION,"undo_begin_action: action_size was <= 0\n"));
  array_error = xGArr_New (&(action->node_list), action_size, 64);
  if (array_error!=xGArr_tErr_NoErr)
    ErrorReturn(ERROR_ARRAY,(ERROR_ARRAY,"undo_begin_action: xGArr_New failed\n"));

  /* if there was a list in the last slot, delete it */
  if (undo_list[UNDO_LIST_POS_LAST]!=NULL) {
    array_error = xGArr_Delete (&(undo_list[UNDO_LIST_POS_LAST]->node_list));
    if (array_error!=xGArr_tErr_NoErr)
      ErrorPrintf(0,"undo_begin_action: xGArr_Delete failed\n");
    free (undo_list[UNDO_LIST_POS_LAST]);
  }

  /* move the undo lists down */
  for (undo_index=UNDO_LIST_POS_LAST; 
       undo_index>UNDO_LIST_POS_FIRST; 
       undo_index--) {
    undo_list[undo_index] = undo_list[undo_index-1];
  }
    
  /* save the new action in the list */
  undo_list[UNDO_LIST_POS_FIRST] = action;

  /* set our state */
  undo_list_state = UNDO_LIST_STATE_OPEN;

  return(NO_ERROR);
}

int undo_finish_action()
{
  /* set our state */
  undo_list_state = UNDO_LIST_STATE_CLOSED;

  undo_send_first_action_name ();
  
  return(NO_ERROR);
}

int undo_send_first_action_name()
{
  char* string = NULL;
  char command[1024] = "";

  if (g_interp==NULL)
    return(NO_ERROR);

  /* get the undo action string and send it to the interface */
  if (undo_list[UNDO_LIST_POS_FIRST]!=NULL)
    {
      string = undo_get_action_string (undo_list[UNDO_LIST_POS_FIRST]->undo_type);
    } else {
      string = undo_get_action_string (UNDO_NONE);
    }

  if (string==NULL)
    ErrorReturn(ERROR_UNDO_ACTION,(ERROR_UNDO_ACTION,"undo_send_first_action_name: undo_get_action_string returned null\n"));

  /* build and send the command */
  sprintf (command, "UpdateUndoItemLabel \"%s\"", string);
  Tcl_Eval (g_interp, command);

  return(NO_ERROR);
}

int undo_copy_action_node(void* node) 
{
  xGArr_tErr array_error = xGArr_tErr_NoErr;

  /* make sure the list is open */
  if (undo_list_state!=UNDO_LIST_STATE_OPEN)
    ErrorReturn(ERROR_UNDO_LIST_STATE,(ERROR_UNDO_LIST_STATE,"undo_copy_action_node: undo list state was not open\n"));

  /* copy the node to the action node list */
  array_error = xGArr_Add (undo_list[UNDO_LIST_POS_FIRST]->node_list, node);
  if (array_error!=xGArr_tErr_NoErr)
    ErrorReturn(ERROR_ARRAY,(ERROR_ARRAY,"undo_copy_action_node: xGArr_Add failed\n"));

  return(NO_ERROR);
}

int undo_do_first_action()
{
  int error = NO_ERROR;
  int undo_index = 0;
  xGArr_tErr array_error;

  /* return if we have no list */
  if (undo_list[UNDO_LIST_POS_FIRST]==NULL)
    return(NO_ERROR);

  /* make sure the list is closed */
  if (undo_list_state!=UNDO_LIST_STATE_CLOSED)
    ErrorReturn(ERROR_UNDO_LIST_STATE,(ERROR_UNDO_LIST_STATE,"undo_do_first_action: undo list state was not closed\n"));

  /* get the action and switch on its type. call the proper handler. if it
     returns an error, print a msg but don't bail. */
  switch (undo_list[UNDO_LIST_POS_FIRST]->undo_type)
    {
    case UNDO_CUT: 
      error = undo_do_action_cut (undo_list[UNDO_LIST_POS_FIRST]);
      break;
    default:
      ErrorReturn(ERROR_UNDO_ACTION,(ERROR_UNDO_ACTION,"undo_do_first_action: invalid action type in non-null action\n"));
      break;
    }
  if (error!=NO_ERROR)
    ErrorPrintf(ERROR_UNDO_ACTION,"undo_do_first_action: undo handler returned an error\n");

  /* delete that action */
  array_error = xGArr_Delete (&(undo_list[UNDO_LIST_POS_FIRST]->node_list) );
  if (array_error!=xGArr_tErr_NoErr)
    ErrorPrintf(ERROR_ARRAY,"undo_do_first_action: xGArr_Delete failed\n");
  free (undo_list[UNDO_LIST_POS_FIRST]);

  /* remove the action from the list and bump everything up. set the
     last pos to null. */
  for (undo_index=UNDO_LIST_POS_FIRST; 
       undo_index<UNDO_LIST_POS_LAST; 
       undo_index++) {
    undo_list[undo_index] = undo_list[undo_index+1];
  }
  undo_list[UNDO_LIST_POS_LAST] = NULL;

  /* refresh the undo menu item with the new first item's name */
  undo_send_first_action_name();

  /* redraw the view */
  vertex_array_dirty = 1;
  rip_faces();
  redraw();

  return(NO_ERROR);
}

int undo_new_action_cut(int cut_type, int index, int rip_value)
{
  UNDO_CUT_NODE cut_action;
  
  /* make sure the list is open */
  if (undo_list_state!=UNDO_LIST_STATE_OPEN)
    ErrorReturn(ERROR_UNDO_LIST_STATE,(ERROR_UNDO_LIST_STATE,"undo_new_action_cut: undo list state was not open\n"));

  /* fill out the fields */
  cut_action.cut_type = cut_type;
  cut_action.index = index;
  cut_action.rip_value = rip_value;

  /* copy it to the list */
  undo_copy_action_node ((void*)&cut_action);

  return(NO_ERROR);
}

int undo_do_action_cut(UNDO_ACTION* action)
{
  UNDO_CUT_NODE cut_action;
  xGArr_tErr array_error;

  if (action==NULL)
    ErrorReturn(ERROR_UNDO_ACTION,(ERROR_UNDO_ACTION,"undo_do_action_cut: action was null\n"));

  if (action->undo_type!=UNDO_CUT)
    ErrorReturn(ERROR_UNDO_ACTION,(ERROR_UNDO_ACTION,"undo_do_action_cut: action type was not cut\n"));

  /* go thru every node in the array... */
  xGArr_ResetIterator (action->node_list);
  array_error = xGArr_tErr_NoErr;
  while ((array_error=xGArr_NextItem(action->node_list,(void*)&cut_action))
   ==xGArr_tErr_NoErr) 
    {
      /* switch on the cut action type and set the face or vertex rip flag to 
   the stored value */
      switch (cut_action.cut_type)
  {
  case UNDO_CUT_VERTEX:
    set_vertex_rip (cut_action.index, cut_action.rip_value, FALSE);
    break;
  case UNDO_CUT_FACE:
    set_face_rip (cut_action.index, cut_action.rip_value, FALSE);
    break;
  default:
    ErrorPrintf(ERROR_UNDO_ACTION,"undo_do_action_cut: invalid cut type\n");
  }
    }

  return(NO_ERROR);
}

/* ---------------------------------------------------------------------- */

/* -------------------------------------------- functional volume support */

int func_initialize()
{
  xGArr_tErr array_error = xGArr_tErr_NoErr;

  /* init our list */
  array_error = xGArr_New (&func_selected_ras, 
         sizeof(FUNC_SELECTED_VOXEL), 64);
  if (array_error!=xGArr_tErr_NoErr)
    ErrorReturn(ERROR_ARRAY,(ERROR_ARRAY,"func_initialize: xGArr_New failed\n"));

  func_use_timecourse_offset = FALSE;
  func_sub_prestim_avg = FALSE;

  return(ERROR_NONE);
}

int func_load_timecourse (char* dir, char* stem, char* registration)
{
  FunD_tErr volume_error;
  char tcl_cmd[1024];
  float time_resolution;

  if (dir==NULL||stem==NULL)
    ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM,"func_load_timecourse: dir or stem was null\n"));
  
  /* delete existing */
  if (func_timecourse!=NULL)
    {
      volume_error = FunD_Delete(&func_timecourse);
      if (volume_error!=FunD_tErr_NoError)
  ErrorPrintf(func_convert_error(volume_error),"func_load_timecourse: error in FunD_Delete\n");
    }

  /* create new volume */
  volume_error = FunD_New (&func_timecourse, dir, stem, NULL, registration );
  if (volume_error!=FunD_tErr_NoError)
    {
      printf("### surfer: couldn't load %s/%s\n",dir,stem);
      ErrorReturn(func_convert_error(volume_error),(func_convert_error(volume_error),"func_load_timecourse: error in FunD_New\n"));
    }
  
  printf("surfer: loaded timecourse %s/%s\n",dir,stem);

  /* get the time res, num conditions, and num presitm points */
  FunD_GetNumPreStimTimePoints (func_timecourse, &func_num_prestim_points);
  FunD_GetTimeResolution (func_timecourse, &time_resolution);
  FunD_GetNumConditions (func_timecourse, &func_num_conditions);
  func_time_resolution = (double)time_resolution;

  /* if we have a tcl shell, send the tcl updates. */
  if (g_interp!=NULL)
    {
      /* send the number of conditions */
      sprintf (tcl_cmd, "Graph_SetNumConditions %d", func_num_conditions);
      Tcl_Eval(g_interp,tcl_cmd);

      /* show the graph window */
      Tcl_Eval(g_interp,"Graph_ShowWindow");

      /* enable the related menu items */
      enable_menu_set (MENUSET_TIMECOURSE_LOADED, 1);
    }


  return(ERROR_NONE);
}

int func_load_timecourse_offset (char* dir, char* stem, char* registration)
{
  FunD_tErr volume_error;
  
  if (dir==NULL||stem==NULL)
    ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM,"func_load_timecourse_offset: dir or stem was null\n"));
  
  /* delete existing */
  if (func_timecourse_offset!=NULL)
    {
      volume_error = FunD_Delete(&func_timecourse_offset);
      if (volume_error!=FunD_tErr_NoError)
  ErrorPrintf(func_convert_error(volume_error),"func_load_timecourse_offset: error in FunD_Delete\n");
    }
  
  /* create new volume */
  volume_error = FunD_New (&func_timecourse_offset, 
         dir, stem, NULL, registration );
  if (volume_error!=FunD_tErr_NoError)
    ErrorReturn(func_convert_error(volume_error),(func_convert_error(volume_error),"func_load_timecourse_offset: error in FunD_New\n"));

  /* enable offset display */
  func_use_timecourse_offset = TRUE;

  printf("surfer: loaded timecourse offset %s/%s\n",dir,stem);

  /* if we have a tcl shell, notify the graph we loaded an offset. */
  if (g_interp!=NULL)
    {
      /* turn on the offset options */
      Tcl_Eval(g_interp, "Graph_ShowOffsetOptions 1");
    }

  return(ERROR_NONE);
}

int func_select_marked_vertices()
{
#if 0
  int vmarked;
  VERTEX* v = NULL;

  for (vmarked = 0 ; vmarked < nmarked ; vmarked++)
    {
      v = &(mris->vertices[marked[vmarked]]);
      func_select_ras (v->origx,v->origy,v->origz);
    }
#else 
  int vno, count;
  VERTEX* v = NULL;

  count = 0;
  for (vno = 0 ; vno < mris->nvertices ; vno++)
    {
      v = &(mris->vertices[vno]);
      if (v->marked)
  {
    func_select_ras (v->origx,v->origy,v->origz);
    count++;
  }
    }
  printf("surfer: averaging %d voxels\n",count);
#endif
  return(ERROR_NONE);
}

int func_select_label()
{
  int lno;
  int count;
  LV* lv = NULL;
  VERTEX* v = NULL;

  if (area==NULL)
    {
      printf ("surfer: label not loaded!\n");
      return(ERROR_NONE);
    }
  
  count = 0;
  for (lno = 0 ; lno < area->n_points ; lno++)
    {
      lv = &(area->lv[lno]);
      if (lv->vno >= 0 && lv->vno < mris->nvertices)
  {
    v = &(mris->vertices[lv->vno]);
    func_select_ras (v->origx,v->origy,v->origz);
    count++;
  }
    }
  
  printf("surfer: averaging %d voxels\n",count);
  return(ERROR_NONE);
}

int func_clear_selection() 
{
  xGArr_tErr array_error = xGArr_tErr_NoErr;

  if (func_selected_ras==NULL)
    ErrorReturn(ERROR_NOT_INITED,(ERROR_NOT_INITED,"func_clear_selection: func_selected_ras is null\n"));
  
  /* clear our selection list */
  array_error = xGArr_Clear (func_selected_ras);
  if (array_error!=xGArr_tErr_NoErr)
    ErrorReturn(ERROR_ARRAY,(ERROR_ARRAY,"func_clear_selection: xGArr_Clear failed\n"));
  
  return(ERROR_NONE);
}

int func_select_ras (float x, float y, float z)
{
  xGArr_tErr array_error = xGArr_tErr_NoErr;
  FUNC_SELECTED_VOXEL voxel;
  
  if (func_selected_ras==NULL)
    ErrorReturn(ERROR_NOT_INITED,(ERROR_NOT_INITED,"func_select_ras: func_selected_ras is null\n"));
  
  /* build a voxel */
  voxel.x = x;
  voxel.y = y;
  voxel.z = z;

  /* add this voxel to the list */
  array_error = xGArr_Add (func_selected_ras, &voxel);
  if (array_error!=xGArr_tErr_NoErr)
    ErrorReturn(ERROR_ARRAY,(ERROR_ARRAY,"func_select_ras: xGArr_Add failed\n"));
  
  return(ERROR_NONE);
}

int func_graph_timecourse_selection () {
  
  int cond,num_conditions;
  int tp,num_timepoints;
  int num_good_voxels;
  float* values;
  float* deviations;
  char* tcl_cmd;
  int tcl_cmd_size;
  FunD_tErr func_error = FunD_tErr_NoError;
  float second;
  int tcl_error;

 /* make sure we have a volume */
  if (func_timecourse==NULL)
    {
      printf ("surfer: time course not loaded\n");
      return (ERROR_NONE);
    }

  /* make sure the graph window is open. */
  Tcl_Eval(g_interp,"Graph_ShowWindow");

  /* get the number of values and allocate storage arrays. */
  FunD_GetNumTimePoints (func_timecourse,&num_timepoints);
  values = calloc (num_timepoints,sizeof(float));
  if (values==NULL)
    ErrorReturn(ERROR_NOMEMORY,(ERROR_NOMEMORY,"func_graph_timecourse_selection: calloc(%d,float) failed for values\n",num_timepoints));  
  deviations = calloc (num_timepoints,sizeof(float));
  if (deviations==NULL)
    ErrorReturn(ERROR_NOMEMORY,(ERROR_NOMEMORY,"func_graph_timecourse_selection: calloc(%d,float) failed for deviations\n",num_timepoints));  
  
  /* allocate the argument string.  */
  tcl_cmd_size = (num_timepoints * knLengthOfGraphDataItem) + knLengthOfGraphDataHeader;
  tcl_cmd = (char*)malloc(sizeof(char)*tcl_cmd_size);
  if(tcl_cmd==NULL ) 
    ErrorReturn(ERROR_NOMEMORY,(ERROR_NOMEMORY,"func_graph_timecourse_selection: failed to alloc %d char string\n",tcl_cmd_size));
  
  /* clear the graph so if nothing draws, we won't have any old stuff there. */
  Tcl_Eval(interp,"Graph_ClearGraph");
  Tcl_Eval(interp,"Graph_BeginData");
  
  /* get the num of conditions. for each one... */
  FunD_GetNumConditions (func_timecourse,&num_conditions);
  for (cond=0;cond<num_conditions;cond++) 
    {
      
      /* get the average values for this condition. we also find out from
   this function how many of our selected voxels were actually in 
   functional space. */
      func_calc_avg_timecourse_values (cond, &num_good_voxels, 
               values, deviations);
      
      /* if we had any good voxels, build a list of values and send to graph */
      if (num_good_voxels>0) 
  {
    /* write the cmd name, condition number and first brace */
    sprintf (tcl_cmd, "Graph_SetPointsData %d {", cond);
    
    /* for each time point... */
    for (tp=0;tp<num_timepoints;tp++)
      {
        
        /* convert to a second. */
        func_error = 
    FunD_ConvertTimePointToSecond (func_timecourse, tp, &second);
        if(func_error!=FunD_tErr_NoError)
    ErrorPrintf(func_convert_error(func_error),"func_graph_timecourse_selection: error in FunD_ConvertTimePointToSecond tp=%d\n",tp);
        
        /* write the second and value to the arg list */
        sprintf (tcl_cmd, "%s %1.1f %2.5f", tcl_cmd, second, values[tp]);
      }
    
    /* write the last brace and send the cmd. */
    sprintf (tcl_cmd, "%s}", tcl_cmd);
    Tcl_Eval (interp,tcl_cmd);
    
    /* send the error bars. write the cmd name. */
    sprintf (tcl_cmd, "Graph_SetErrorData %d {", cond);
    
    /* for each time point, write the deviation value. */
    for (tp=0;tp<num_timepoints;tp++)
      sprintf (tcl_cmd, "%s %2.5f", tcl_cmd, deviations[tp]);
    
    /* write the last brace and send the cmd. */
    sprintf (tcl_cmd, "%s}", tcl_cmd);
    Tcl_Eval (interp,tcl_cmd);
  }
    }
  
  tcl_error = Tcl_Eval(interp,"Graph_EndData");
  if (tcl_error != TCL_OK)
    printf("func_graph_timecourse_selection: error in Graph_EndData: %s\n",interp->result);
  
  return(ERROR_NONE);
}

int func_print_timecourse_selection (char* fname)
{
  FILE* fp = NULL;
  int cond,num_conditions;
  int tp,num_timepoints;
  int num_good_voxels;
  float* values;
  float* deviations;

  if (fname==NULL)
    ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM,"func_print_timecourse_selection: file name was null\n"));

  /* make sure we have a volume */
  if (func_timecourse==NULL)
    ErrorReturn(ERROR_NOT_INITED,(ERROR_NOT_INITED,"func_print_timecourse_selection: func_timecourse is null\n"));
  
  /* get the number of values and allocate storage arrays. */
  FunD_GetNumTimePoints (func_timecourse,&num_timepoints);
  values = calloc (num_timepoints,sizeof(float));
  if (values==NULL)
    ErrorReturn(ERROR_NOMEMORY,(ERROR_NOMEMORY,"func_print_timecourse_selection: calloc(%d,float) failed for values\n",num_timepoints));  

  deviations = calloc (num_timepoints,sizeof(float));
  if (deviations==NULL)
    ErrorReturn(ERROR_NOMEMORY,(ERROR_NOMEMORY,"func_print_timecourse_selection: calloc(%d,float) failed for deviations\n",num_timepoints));  


  fp = fopen (fname, "w");
  if (fp==NULL)
    ErrorReturn(ERROR_NOFILE,(ERROR_NOFILE,"func_print_timecourse_selection: file %s couldn't be opened\n", fname));

  /* get the num of conditions. for each one... */
  FunD_GetNumConditions (func_timecourse,&num_conditions);
  for (cond=0;cond<num_conditions;cond++) 
    {
      fprintf (fp,"Condition %d/%d\n",cond,num_conditions);
      
      /* get the average values for this condition. we also find out from
   this function how many of our selected voxels were actually in 
   functional space. */
      func_calc_avg_timecourse_values (cond, &num_good_voxels, 
               values, deviations);
      
      fprintf (fp,"%d voxels in range.\n",num_good_voxels);
      
      /* if we had any good voxels, print out a summary */
      if (num_good_voxels>0) 
  for (tp=0;tp<num_timepoints;tp++)
    fprintf (fp, "%d\t%d\t%f\t%f\n",
       tp, num_timepoints, values[tp], deviations[tp]);
      
    }
  
  fclose (fp);

  return(ERROR_NONE);
}

int func_calc_avg_timecourse_values (int condition, int* num_good_voxels,
             float values[], float deviations[] )
{

  int tp,num_timepoints;
  float* sums;
  FUNC_SELECTED_VOXEL selected_voxel;
  xVoxel voxel;
  xGArr_tErr array_error = xGArr_tErr_NoErr;
  FunD_tErr func_error = FunD_tErr_NoError;
  float offset,offset_sum;
  
  /* find the number of time points. */
  FunD_GetNumTimePoints (func_timecourse,&num_timepoints);
  
  /* allocate sums */
  sums = (float*) calloc (num_timepoints,sizeof(float));
  if (sums==NULL)
    ErrorReturn(ERROR_NOMEMORY,(ERROR_NOMEMORY,"func_calc_avg_timecourse_values: calloc(%d,float) failed\n",num_timepoints));
  
  /* no good values yet */
  (*num_good_voxels) = 0;
  
  /* get the values at all time points. add them
     in the sums array. keep track of how many we have. */
  offset_sum = 0;
  xGArr_ResetIterator (func_selected_ras);
  array_error = xGArr_tErr_NoErr;
  while ((array_error=xGArr_NextItem(func_selected_ras,(void*)&selected_voxel))
   ==xGArr_tErr_NoErr) 
    {
      
      /* convert to an xvoxel */
      xVoxl_SetFloat (&voxel,
          selected_voxel.x,selected_voxel.y,selected_voxel.z);
      
      /* get all values at this voxel */
      func_error = FunD_GetDataAtRASForAllTimePoints( func_timecourse, &voxel, 
                  condition, values );
      
      /* if it wasn't out of bounds... */
      if (func_error==FunD_tErr_NoError) {
  
  /* if we are displaying offsets and we have offset data... */
  if (func_use_timecourse_offset && func_timecourse_offset!=NULL) 
    {
      /* get the offset at this value. only one plane in offset data. */
      func_error = FunD_GetDataAtRAS( func_timecourse_offset, 
              &voxel, 0, 0, &offset );
      if (func_error==FunD_tErr_NoError )
        {
    
    /* divide all functional values by the offset and mult by 100 to 
       get a percent */
    for (tp=0;tp<num_timepoints;tp++) 
      values[tp] = (values[tp]/offset)*100.0;
    
    /* get a sum off the offset. */
    offset_sum += offset;
        }
    }
  
  /* add all values to our sums array */
  for (tp=0;tp<num_timepoints;tp++) 
    sums[tp] += values[tp];
  
  /* inc our count */
  (*num_good_voxels)++;
      }
    }
  
  /* if we don't have any values at this point, our whole selections
     is out of range. */
  if ((*num_good_voxels)==0)
    return(ERROR_NONE);
  
  /* divide everything by the number of values to find the average */
  for (tp=0;tp<num_timepoints;tp++) 
    values[tp] = sums[tp] / (float)(*num_good_voxels);
  
  /* if we have offset values, divide the offset sum by the number of
     values. */
  if (func_use_timecourse_offset && func_timecourse_offset!=NULL) 
    offset = offset_sum / (float)(*num_good_voxels);
  
  /* if there is error data present.. */
  if (FunD_IsErrorDataPresent(func_timecourse)) 
    {
      
      /* get the deviations at all time points */
      func_error = FunD_GetDeviationForAllTimePoints (func_timecourse,
                  condition, deviations);
      
      if (func_error!=FunD_tErr_NoError)
  ErrorPrintf(func_convert_error(func_error),"func_calc_avg_timecourse_values: error in FunD_GetDeviationForAllTimePoints\n");
      
      /* if we have offset values... */
      if (func_use_timecourse_offset && func_timecourse_offset!=NULL) 
  {
    
    /* divide all deviations by the offset and mult by 100 to 
       get a percent */
    for (tp=0;tp<num_timepoints;tp++)
      deviations[tp] = (deviations[tp]/offset)*100.0;
  }
    } else {
      
      /* fill deviations with 0s */
      for (tp=0;tp<num_timepoints;tp++)
  deviations[tp] = 0;
    }
  
  return(ERROR_NONE);
}

int func_convert_error (FunD_tErr volume_error)
{
  int error = ERROR_NONE;
  switch (volume_error)
    {
    case FunD_tErr_PathNotFound:
    case FunD_tErr_CouldntGuessStem:
    case FunD_tErr_DataNotFound:
    case FunD_tErr_HeaderNotFound:
    case FunD_tErr_UnrecognizedHeaderFormat:
    case FunD_tErr_QuestionableHeaderFormat:
    case FunD_tErr_CouldntDetermineDataType:
    case FunD_tErr_SliceFileNotFound:
    case FunD_tErr_ErrorReadingSliceData:
      error = ERROR_NOFILE;
      break;
    case FunD_tErr_CouldntAllocateVolume:
    case FunD_tErr_CouldntAllocateStorage:
    case FunD_tErr_CouldntAllocateMatrix:
      error = ERROR_NO_MEMORY;
      break;
    default:
      error = ERROR_FUNC;
      break;
    }
  return(error);
}
/* ---------------------------------------------------------------------- */

/* --------------------------------------------------- scalar value mgmnt */

int sclv_initialize () 
{
  int field;

  /* no layers loaded, clear all the stuff. */
  for (field = 0; field < NUM_SCALAR_VALUES; field++)
  {
    sclv_field_info[field].is_binary_volume = FALSE;
    sclv_field_info[field].cur_timepoint = -1;
    sclv_field_info[field].cur_condition = -1;
    sclv_field_info[field].volume = NULL;
    sclv_field_info[field].fthresh = fthresh;
    sclv_field_info[field].fmid = fmid;
    sclv_field_info[field].foffset = foffset;
    sclv_field_info[field].fslope = fslope;
  }

  return (ERROR_NONE);
}

int sclv_unload_field (int field) 
{

  FunD_tErr volume_error;

  if (field < 0 || field > NUM_SCALAR_VALUES)
    ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM,"sclv_unload_field: field was out of bounds: %d)",field));

  /* if we have a binary volume, delete it */
  if (sclv_field_info[field].is_binary_volume && 
      sclv_field_info[field].volume != NULL )
    {
      volume_error = FunD_Delete (&(sclv_field_info[field].volume));
      if (volume_error!=FunD_tErr_NoError)
  ErrorReturn(func_convert_error(volume_error),(func_convert_error(volume_error),"sclv_unload_field: error in FunD_Delete\n"));

      sclv_field_info[field].is_binary_volume = FALSE;

      /* force a repaint next load */
      sclv_field_info[field].cur_timepoint = -1;
      sclv_field_info[field].cur_condition = -1;
    }
  
  return (ERROR_NONE);
}

int
sclv_calc_frequencies(int field)
{
  int timepoint, condition;
  float value;
  int bin;
  float num_values;
  VERTEX* v;
  int vno;
  float valPerBin;
  
  if (field < 0 || field > NUM_SCALAR_VALUES)
    ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM,"sclv_unload_field: field was out of bounds: %d)",field));

  sclv_field_info[field].num_freq_bins = SCLV_NUM_FREQUENCY_BINS;
      
  /* allocate storage for each time point and condition... */
  sclv_field_info[field].frequencies = 
    calloc( sclv_field_info[field].num_conditions, sizeof(int**) );
  for (condition = 0; 
       condition < sclv_field_info[field].num_conditions; condition++)
    {
      sclv_field_info[field].frequencies[condition] = 
  calloc( sclv_field_info[field].num_timepoints, sizeof(int*) );

      for (timepoint = 0; 
     timepoint < sclv_field_info[field].num_timepoints; timepoint++)
  {

    /* allocate an array of num_freq_bins ints */
    sclv_field_info[field].frequencies[condition][timepoint] = 
      calloc( sclv_field_info[field].num_freq_bins, sizeof(int) );
    
    /* if this is a binary volume, it will paint the proper tp
       and condition into the vertex fields as well as calculate
       the correct max/mins. */
    sclv_set_timepoint_of_field (field, timepoint, condition);
      
    /* get the value range. */
    num_values = (sclv_field_info[field].max_value - 
      sclv_field_info[field].min_value) + 1;
    valPerBin = num_values / (float)sclv_field_info[field].num_freq_bins;
    
    /* for each vertex, get the scalar value. find the bin it
       should go in and inc the count in that bin. */
    for (vno = 0 ; vno < mris->nvertices ; vno++)
      {
        v = &mris->vertices[vno] ;
        sclv_get_value (v, field, &value);
        bin = (value - sclv_field_info[field].min_value) / valPerBin;
        sclv_field_info[field].frequencies[condition][timepoint][bin]++;
      }
  }
    }
  
#if 0
  printf ("field %d ---------------------- \n", field);
  for (condition = 0; 
       condition < sclv_field_info[field].num_conditions; condition++)
    {
      printf ("\t---------------------- \n");
      for (timepoint = 0; 
     timepoint < sclv_field_info[field].num_timepoints; timepoint++)
  {
    printf ("\t---------------------- \n");
    for (bin = 0; bin < sclv_field_info[field].num_freq_bins; bin++)
      {
        printf ("\tcond %d tp %d bin %d = %d\n",
          condition, timepoint, bin,
    sclv_field_info[field].frequencies[condition][timepoint][bin]);
      }
  }
    }
  printf ("------------------------------- \n");
#endif

  return (ERROR_NONE);
}

int sclv_set_current_field (int field)
{
  if (field < 0 || field > NUM_SCALAR_VALUES)
    ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM,"sclv_set_current_field: field was out of bounds: %d)",field));

  /* save the current threshold */
  sclv_field_info[sclv_current_field].fthresh = fthresh;
  sclv_field_info[sclv_current_field].fmid = fmid;;
  sclv_field_info[sclv_current_field].foffset = foffset;;
  sclv_field_info[sclv_current_field].fslope = fslope;
  sclv_field_info[sclv_current_field].cur_condition = sclv_cur_condition;
  sclv_field_info[sclv_current_field].cur_timepoint = sclv_cur_timepoint;

  /* update the field */
  sclv_current_field = field;

  /* set the shared vars. */
  fthresh = sclv_field_info[sclv_current_field].fthresh;
  fmid = sclv_field_info[sclv_current_field].fmid;;
  foffset = sclv_field_info[sclv_current_field].foffset;;
  fslope = sclv_field_info[sclv_current_field].fslope;
  sclv_cur_condition = sclv_field_info[sclv_current_field].cur_condition;
  sclv_cur_timepoint = sclv_field_info[sclv_current_field].cur_timepoint;
  sclv_num_timepoints = sclv_field_info[sclv_current_field].num_timepoints;
  sclv_num_conditions = sclv_field_info[sclv_current_field].num_conditions;
  sclv_value_min = sclv_field_info[sclv_current_field].min_value;
  sclv_value_max = sclv_field_info[sclv_current_field].max_value;

  /* send the info for this field */
  sclv_send_current_field_info ();

  return (ERROR_NONE);
}

int sclv_send_current_field_info ()
{

  char cmd[1024];

#if 0
  printf("sending info for field=%d\n\tmin=%f max=%f\n\tfthresh=%f fmid=%f fslope=%f\n\ttp=%d cn=%d ntps+%d ncns=%d\n", sclv_current_field, sclv_value_min, sclv_value_max, fthresh, fmid, fslope, sclv_cur_timepoint, sclv_cur_condition, sclv_num_timepoints, sclv_num_conditions);
#endif

  /* Send the current histogram info here as well. */
  sclv_send_histogram (sclv_current_field);

  if (g_interp)
    {
      sprintf (cmd, "UpdateLinkedVarGroup overlay");
      Tcl_Eval (g_interp, cmd);
      sprintf (cmd, "UpdateOverlayDlogInfo");
      Tcl_Eval (g_interp, cmd);
    }

  return (ERROR_NONE);
}

int sclv_set_timepoint_of_field (int field, 
         int timepoint, int condition)
{
  FunD_tErr volume_error;
  int vno;
  VERTEX* v;
  xVoxel voxel;
  float func_value;
  
  if (field < 0 || field > NUM_SCALAR_VALUES)
    ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM,"sclv_set_timepoint_of_field: field was out of bounds: %d)",field));

  /* check if this field has a binary volume */
  if (sclv_field_info[field].is_binary_volume == FALSE )
    {
      /* commented out because tksurfer.tcl will call this function
         even when there is a .w file in this layer. */
      /*      ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM,"sclv_set_timepoint_of_field: field %d doesn't have a binary volume",field)); */
      return (ERROR_NONE);
    }
  
  /* make sure it actually has one */
  if (sclv_field_info[field].is_binary_volume && 
      sclv_field_info[field].volume == NULL )
    {
      ErrorReturn(ERROR_BADPARM,(ERROR_BADPARM,"sclv_set_timepoint_of_field: field %d thinks it has a binary volume but doesn't really",field));
    }  
  
  /* check the timepoint and condition. if they're not what we're already
     using...*/
  if (timepoint != sclv_field_info[field].cur_timepoint ||
      condition != sclv_field_info[field].cur_condition )
    {
      
      /* for each vertex, grab a value out of the blfoat volume and stick it
   in the field */
      for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    
    /* skip ripped verts */
    if (v->ripflag)
      continue ;
    
    /* if the voxel is valid here, use the value, else set to 0. */
    xVoxl_SetFloat (&voxel, v->origx, v->origy, v->origz);
    volume_error = FunD_GetDataAtRAS(sclv_field_info[field].volume,
             &voxel, condition, timepoint,
             &func_value);
    if (volume_error == FunD_tErr_NoError)
      {
        sclv_set_value (v, field, func_value);
      } else {
        sclv_set_value (v, field, 0.0);
      }
  }
      
      /* save the timepoint and condition. */
      sclv_field_info[field].cur_timepoint = timepoint;
      sclv_field_info[field].cur_condition = condition;
      
      /* send the info for the current field */
      if (field == sclv_current_field)
  sclv_send_current_field_info();
    }
  
  return (ERROR_NONE);
}

int sclv_set_current_threshold_from_percentile (float thresh, float mid, 
            float max) 
{
  sclv_set_threshold_from_percentile(sclv_current_field, 
             thresh, mid, max);
  
  sclv_send_current_field_info();

  return (ERROR_NONE);
}

int sclv_set_threshold_from_percentile (int field, float thresh, float mid, 
          float max)
{
  float thresh_value, mid_value, max_value;

  if (field < 0 || field >= NUM_SCALAR_VALUES)
    return (ERROR_BADPARM);
  if (thresh < 0 || thresh > 100.0)
    return (ERROR_BADPARM);
  if (mid < 0 || mid > 100.0)
    return (ERROR_BADPARM);
  if (max < 0 || max > 100.0)
    return (ERROR_BADPARM);

  sclv_get_value_for_percentile (field, thresh, &thresh_value);
  sclv_get_value_for_percentile (field, mid, &mid_value);
  sclv_get_value_for_percentile (field, max, &max_value);

  sclv_field_info[field].fthresh = thresh_value;
  sclv_field_info[field].fmid = mid_value;
  if (max_value - mid_value < epsilon)
    {
      sclv_field_info[field].fslope = 1.0;
    } else {
      sclv_field_info[field].fslope = 1.0 / (max_value - mid_value);
    }

  vertex_array_dirty = 1;

  printf ("fthresh %.2f fmid %.2f max %.2f slope %.2f\n",
    thresh_value, mid_value, max_value, sclv_field_info[field].fslope);

  return (ERROR_NONE);
}

int sclv_get_value_for_percentile (int field, float percentile, float* value)
{
  int target_count;
  int bin, sum;

  if (field < 0 || field >= NUM_SCALAR_VALUES)
    return (ERROR_BADPARM);
  if (percentile < 0 || percentile > 100.0)
    return (ERROR_BADPARM);

  target_count = (float)mris->nvertices * (percentile / 100.0);

  sum = 0;
  bin = 0;
  while (sum < target_count && bin < sclv_field_info[field].num_freq_bins)
    {
      sum += sclv_field_info[field].frequencies[sclv_field_info[field].cur_condition][sclv_field_info[field].cur_timepoint][bin];
      bin++;
    }
  
  *value = sclv_field_info[field].min_value +
    ( ((sclv_field_info[field].max_value - sclv_field_info[field].min_value + 1) * bin) / sclv_field_info[field].num_freq_bins);
  
  return (ERROR_NONE);
}

int sclv_copy_view_settings_from_field (int field, int fromfield)
{
  if (field < 0 || field >= NUM_SCALAR_VALUES)
    return (ERROR_BADPARM);
  if (fromfield < 0 || fromfield >= NUM_SCALAR_VALUES)
    return (ERROR_BADPARM);

  sclv_field_info[field].fthresh = sclv_field_info[fromfield].fthresh;
  sclv_field_info[field].fmid = sclv_field_info[fromfield].fmid;
  sclv_field_info[field].foffset = sclv_field_info[fromfield].foffset;
  sclv_field_info[field].fslope = sclv_field_info[fromfield].fslope;

  /* if this is the current field, update the shared variables. */
  if (field == sclv_current_field)
    {
      fthresh = sclv_field_info[sclv_current_field].fthresh;
      fmid = sclv_field_info[sclv_current_field].fmid;;
      foffset = sclv_field_info[sclv_current_field].foffset;;
      fslope = sclv_field_info[sclv_current_field].fslope;
    }

  return (ERROR_NONE);
}

int sclv_copy_view_settings_from_current_field (int field)
{
  sclv_copy_view_settings_from_field (field, sclv_current_field);

  return (ERROR_NONE);
}

int sclv_swap_fields ( int fielda, int fieldb ) {

  SCLV_FIELD_INFO swap_field;
  char cmd[STRLEN];
  int k;
  float a, b;

  if (fielda < 0 || fielda >= NUM_SCALAR_VALUES)
    return (ERROR_BADPARM);
  if (fieldb < 0 || fieldb >= NUM_SCALAR_VALUES)
    return (ERROR_BADPARM);

  /* swap the field values */
  for (k=0;k<mris->nvertices;k++)
  {
    sclv_get_value(&(mris->vertices[k]), fielda, &a );
    sclv_get_value(&(mris->vertices[k]), fieldb, &b );
    sclv_set_value(&(mris->vertices[k]), fielda, b );
    sclv_set_value(&(mris->vertices[k]), fieldb, a );
  }

  /* swap the field data */
  swap_field = sclv_field_info[fielda];
  sclv_field_info[fielda] = sclv_field_info[fieldb];
  sclv_field_info[fieldb] = swap_field;

  /* swap the field names in the interface */
  sprintf (cmd, "SwapValueLabelNames %d %d", fielda, fieldb);
  Tcl_Eval (g_interp, cmd);

  return (ERROR_NONE);
}

int sclv_send_histogram ( int field ) {

  float increment;
  char *tcl_cmd;
  int condition, timepoint;
  int bin;

  /* calculate the number of values and the increment between
     each value */
  increment = 
    (sclv_field_info[field].max_value - sclv_field_info[field].min_value) / 
    (float)(sclv_field_info[field].num_freq_bins);

  /* start a string of the proper size; give us the length of the
     command, and then 10 characters per number, begin + end +
     increment + num values */
  tcl_cmd = (char*)calloc(21 + (sclv_field_info[field].num_freq_bins + 4) * 10,
        sizeof(char));
  if (NULL == tcl_cmd)
    {
      return (ERROR_NO_MEMORY);
    }

  /* add the command name to the string, the min value, the max value,
     the number of values, and the increment */
  sprintf (tcl_cmd, "UpdateHistogramData %.5f %.5f %.5f %d {",
     sclv_field_info[field].min_value, sclv_field_info[field].max_value,
     increment, sclv_field_info[field].num_freq_bins);

  /* for each frequency bin, add the value. */
  condition = sclv_field_info[field].cur_condition;
  timepoint = sclv_field_info[field].cur_timepoint;
  for (bin = 0; bin < sclv_field_info[field].num_freq_bins; bin++)
    {
      sprintf (tcl_cmd, "%s %d", tcl_cmd,
         sclv_field_info[field].frequencies[condition][timepoint][bin]);
    }

  /* close up the command and send it off */
  sprintf (tcl_cmd, "%s}", tcl_cmd);

  if (g_interp)
    {
      Tcl_Eval (g_interp, tcl_cmd);
    }

  free (tcl_cmd);

  return (ERROR_NONE);
}

int sclv_get_normalized_color_for_value (int field, float value, 
           float *outRed,
           float *outGreen,
           float *outBlue)
{
  GLubyte r, g, b;
  get_color_vals (value, 0, REAL_VAL, &r, &g, &b);
  *outRed = ((float)r / 255.0);
  *outGreen = ((float)g / 255.0);
  *outBlue = ((float)b / 255.0);
  return (ERROR_NONE);
}


/* ---------------------------------------------------------------------- */

/* ------------------------------------------------------ multiple labels */

int labl_initialize () {

  int label;

  /* initialize the array of labels to empty. */
  for (label = 0; label < LABL_MAX_LABELS; label++)
    {
      labl_labels[label].label = NULL;
      labl_labels[label].structure = -1;
      labl_labels[label].r = 0;
      labl_labels[label].g = 0;
      labl_labels[label].b = 0;
      labl_labels[label].visible = 0;
    }
  
  labl_num_labels = 0;
  labl_selected_label = LABL_NONE_SELECTED;
  labl_table = NULL;
  labl_is_border = NULL;
  labl_top_label = NULL;
  labl_cache_is_current = 0;
  labl_draw_style = LABL_STYLE_OPAQUE;
  labl_num_labels_created = 0;
  labl_color_table_name = (char*) calloc (NAME_LENGTH, sizeof(char));
  labl_draw_flag = 1;

  return (ERROR_NONE);
}

int labl_load_color_table (char* fname)
{
  CLUT_tErr clut_err = CLUT_tErr_NoErr;
  int label;
  xColor3n color;
  int structure;
  char structure_label[CLUT_knLabelLen];
  char* structure_label_list = NULL;
  
  /* if a table exists, delete it. */
  if (NULL != labl_table) 
    {
      clut_err = CLUT_Delete (&labl_table);
      if (CLUT_tErr_NoErr != clut_err)
  {
    fprintf (stderr, "labl_load_color_table: CLUT_Delete returned "
       "%d: %s\n", clut_err, CLUT_GetErrorString (clut_err) );
    labl_table = NULL;
  }
    }
  
  /* create a new table. */
  clut_err = CLUT_New (&labl_table, fname);
  if (CLUT_tErr_NoErr != clut_err)
    {
      fprintf (stderr, "labl_load_color_table: CLUT_New returned "
         "%d: %s\n", clut_err, CLUT_GetErrorString (clut_err) );
      return (ERROR_CLUT);
    }
  
  /* get the number of entries. */
  CLUT_GetNumEntries (labl_table, &labl_num_structures);
  printf ("found %d structures\n", labl_num_structures);
  
  /* for each label, if the structure index is > the number of entries
     we have now, set it to 0 and the corresponding color. */
  CLUT_GetColorInt (labl_table, 0, &color);
  for (label = 0; label < labl_num_labels; label++ )
    {
      if (labl_labels[label].structure >= labl_num_structures)
  {
    labl_labels[label].structure = 0;
    labl_labels[label].r = color.mnRed;
    labl_labels[label].g = color.mnGreen;
    labl_labels[label].r = color.mnBlue;
  }
    }
  
  /* save the name of the color table */
  strcpy (labl_color_table_name, fname);
  
  /* if we have a tcl interpretor... */
  if (NULL != g_interp) 
    {
      /* allocate a string long enough for the update command and all
         our labels. */
      structure_label_list = (char*) malloc (CLUT_knLabelLen * 
         labl_num_structures * sizeof(char) );
      if (NULL != structure_label_list)
  {
    
    /* build a string out of all the label names and send them to the
       tcl label list. */
    strcpy (structure_label_list, "LblLst_SetStructures {");
    for (structure = 0; structure < labl_num_structures; structure++ )
      {
        CLUT_GetLabel (labl_table, structure, structure_label);
        sprintf (structure_label_list, "%s %s",
           structure_label_list, structure_label);
      }
    sprintf (structure_label_list, "%s }", structure_label_list);
    Tcl_Eval (g_interp, structure_label_list);
    
    free( structure_label_list );
    
  } else {
    fprintf (stderr, "labl_load_color_table: CLUT_ParseFile returned "
       "%d: %s\n", clut_err, CLUT_GetErrorString (clut_err) );
    return (ERROR_NO_MEMORY);
  }
    }
  
  return (ERROR_NONE);
}

int labl_load (char* fname) 
{
  LABEL* label = NULL;
  int label_index;
  char name[NAME_LENGTH];
  
  if (NULL == fname)
    return (ERROR_BADPARM);
  
  /* load label file. */
  label = LabelRead (pname, fname);
  if (NULL == label)
    {
      return (ERROR_NO_FILE);
    }
  
  /* load the orig vertex positions if we haven't already. */
  if (!origsurfloaded)
    read_orig_vertex_coordinates(orfname) ;
  LabelToOriginal (label, mris);
  
  /* assign mris vertex numbers to unnumbered vertices based on their
     locations. */
  LabelFillUnassignedVertices (mris, label);
  
  /* make a new entry in the label list. */
  labl_add (label, &label_index);
  
  /* set the name to the tail of the filename. make this a free label,
     i.e. with no assigned strucutre. make it visible. */
  FileNameOnly (fname, name);
  labl_set_info (label_index, name, LABL_TYPE_FREE, 1,
     LABL_DEFAULT_COLOR_R, LABL_DEFAULT_COLOR_G, 
     LABL_DEFAULT_COLOR_B );
  
  /* select this label */
  labl_select (label_index);
  
  surface_compiled = 0 ;
  
  return (ERROR_NONE);
}

int labl_save (int index, char* fname)
{   
  LABEL* label = NULL;
  int n;
  
  if (index < 0 || index >= labl_num_labels)
    return (ERROR_BADPARM);
  if (NULL == fname)
    return (ERROR_BADPARM);
  
  /* get our label. */
  label = labl_labels[index].label;
  if (NULL == label)
    return (ERROR_BADPARM);
  
  /* read original vertex positions if we haven't already. */
  if (!origsurfloaded)
    read_orig_vertex_coordinates(orfname) ;
  
  /* fill in the current overlay value in the stat field of our label
     verts. this is for the LabelWrite call. */
  for (n = 0 ; n < label->n_points ; n++) 
    {
      sclv_get_value (&(mris->vertices[label->lv[n].vno]),
          sclv_current_field, &(label->lv[n].stat) );
    }
  
  /* write the label. */
  fprintf (stderr, "writing %d labeled vertices to %s.\n",
     label->n_points, fname) ;
  LabelToOriginal (label, mris);
  LabelWrite (label, fname);
  
  return (ERROR_NONE);
}

int labl_save_all (char* prefix)
{
  int label;
  char fname[NAME_LENGTH];
  
  /* for each label we have, build a decent name based on the prefix
     we got, and save it normally. */
  for (label = 0; label < labl_num_labels; label++ )
    {
      sprintf (fname, "%s-%d", prefix, label);
      labl_save (label, fname);
    }
  
  return (ERROR_NONE);
}

int labl_find_and_set_border (int index)
{
  int label_vno;
  LABEL* label;
  VERTEX* v;
  int neighbor_vno;
  int neighbor_label;
  char* border = NULL;
  
  if (index < 0 || index >= labl_num_labels)
    return (ERROR_BADPARM);
  
  /* make our label border flag array if we haven't already. */
  if (NULL == labl_is_border)
    {
      labl_is_border = (short*) calloc (mris->nvertices, sizeof(short));
      if (NULL == labl_is_border)
  return (ERROR_NO_MEMORY);
    }
  
  /* make an array of border flags for just the outline. */
  border = (char*) calloc (mris->nvertices, sizeof(char));
  
  /* for each vertex in the label... */
  label = labl_labels[index].label;
  for (label_vno = 0; label_vno < label->n_points; label_vno++)
    {  
      labl_is_border[label->lv[label_vno].vno] = 0;
      
      /* get the vno and look at this vertex in the mris. if it has any
   neighbors that are not in the same label... */
      v = &(mris->vertices[label->lv[label_vno].vno]);
      for (neighbor_vno = 0; neighbor_vno < v->vnum; neighbor_vno++ )
  {
    labl_find_label_by_vno (v->v[neighbor_vno], &neighbor_label);
    
    /* it's a border if this vno is in a different (or no)
             label. makr it so in the real border flag array and our
             temp one. */
    if (neighbor_label != index)
      {
        border[label->lv[label_vno].vno] = TRUE;
        labl_is_border[label->lv[label_vno].vno] = 1;
      }
  }
    }
  
  /* go through again and fill neighboring vertices, so our outline is
     2 vertices thick. this time check only the temp array with the
     outer outline, and not already colored in the real array.. */
  for (label_vno = 0; label_vno < label->n_points; label_vno++)
    {  
      v = &(mris->vertices[label->lv[label_vno].vno]);
      for (neighbor_vno = 0; neighbor_vno < v->vnum; neighbor_vno++ )
  {
    if (border[v->v[neighbor_vno]] && 
        0 == labl_is_border[label->lv[label_vno].vno])
      {
        labl_is_border[label->lv[label_vno].vno] = 2;
        break;
      }
  }
    }

  free (border);

  return (ERROR_NONE);
}

int labl_clear_border (int index)
{
  int label_vno;
  LABEL* label;
  
  if (index < 0 || index >= labl_num_labels)
    return (ERROR_BADPARM);
  
  /* make our label border flag array if we haven't already. */
  if (NULL == labl_is_border)
    {
      labl_is_border = (short*) calloc (mris->nvertices, sizeof(short));
      if (NULL == labl_is_border)
  return (ERROR_NO_MEMORY);
    }
  
  /* for each vertex in the label... */
  label = labl_labels[index].label;
  for (label_vno = 0; label_vno < label->n_points; label_vno++)
    labl_is_border[label->lv[label_vno].vno] = 0;
  
  return (ERROR_NONE);
}

int labl_update_label_cache ()
{
  int vno;
  int label_index;
  int label_vno;
  LABEL* label;
  
  /* init the cache if not already done so. */
  if (NULL == labl_top_label)
    {
      labl_top_label = (int*) calloc (mris->nvertices, sizeof(int));
      if (NULL == labl_top_label)
  return (ERROR_NO_MEMORY);
    }
  
  /* clear the cache. */
  for (vno = 0; vno < mris->nvertices; vno++)
    labl_top_label[vno] = -1;
  
  /* for each label.. */
  for (label_index = 0; label_index < labl_num_labels; label_index++)
    {
      label = labl_labels[label_index].label;
      
      /* for each vertex... */
      for (label_vno = 0; label_vno < label->n_points; label_vno++)
  {
    /* set the value in the cache. */
    labl_top_label[label->lv[label_vno].vno] = label_index;
  }
    }
  
  /* cache is now current. */
  labl_cache_is_current = 1;
  
  return (ERROR_NONE);
}

int labl_import_annotation (char *fname) 
{
  CLUT_tErr clut_err = CLUT_tErr_NoErr;
  int mris_err;
  int annotation_vno;
  int vno;
  int annotation;
  int num_verts_in_annotation;
  LABEL* label = NULL;
  int label_vno;
  VERTEX* v = NULL;
  int new_index;
  char name[NAME_LENGTH];
  int r, g, b;
  xColor3n color;
  int structure;
  int* done;
  
  /* init our done array. */
  r = g = b = 255;
  LABL_PACK_ANNOT(r,g,b,annotation);
  done = (int*) calloc( annotation, sizeof(int) );
  if ( NULL == done ) {
    printf( "calloc of size %d failed\n", annotation );
    return (ERROR_NO_MEMORY);
  }
  
  /* read the annotation. */
  for (vno = 0; vno < mris->nvertices; vno++)
    mris->vertices[vno].annotation = 0;
  mris_err = MRISreadAnnotation (mris, fname);
  
  /* check all annotations... */
  for (annotation_vno = 0; annotation_vno < mris->nvertices; annotation_vno++)
    {
      /* get the annotation. if there is one... */
      annotation = mris->vertices[annotation_vno].annotation;
      
      if (annotation) 
  {
    /* get the rgb colors. */
    LABL_UNPACK_ANNOT( annotation, r, g, b );
    
    /* if we haven't imported this label yet... */
    if( !done[annotation] ) 
      {
        
        /* mark it imported. */
        done[annotation] = 1;
        
        /* find out how many verts have this annotation value. */
        num_verts_in_annotation = 0;
        for (vno = 0; vno < mris->nvertices; vno++)
    if (mris->vertices[vno].annotation == annotation)
      num_verts_in_annotation++;
        
        /* make a new label, and go through again, setting the label
     values. */
        label = LabelAlloc(num_verts_in_annotation, NULL, NULL);
        if (NULL != label)
    {
      label->n_points = num_verts_in_annotation;
      label_vno = 0;
      for (vno = 0; vno < mris->nvertices; vno++)
        if (mris->vertices[vno].annotation == annotation)
          {
      v = &mris->vertices[vno];
      label->lv[label_vno].x = v->x;
      label->lv[label_vno].y = v->y;
      label->lv[label_vno].z = v->z;
      label->lv[label_vno].vno = vno;
      label_vno++;
          }
      
      /* convert to original positions. */
      if (!origsurfloaded)
        read_orig_vertex_coordinates(orfname);
      LabelToOriginal (label, mris);
      
      /* add the label to our list. */
      labl_add (label, &new_index);
      
      /* look for a structure index based on this
         color. if we don't find one, give it index -1,
         making it a free label. */
      color.mnRed = r;
      color.mnGreen = g;
      color.mnBlue = b;
      if (NULL != labl_table)
        {
          clut_err = CLUT_GetIndex (labl_table, 
            &color, &structure);
          if (CLUT_tErr_NoErr != clut_err)
      structure = -1;
        }
      else
        {
          structure = -1;
        }
      
      /* make a name for it. if we got a color from the
         color table, get the label, else use the color. */
      if (structure != -1)
        {
          clut_err = CLUT_GetLabel (labl_table, structure, name);
          if (CLUT_tErr_NoErr != clut_err)
      sprintf (name, "Parcellation %d, %d, %d", r, g, b);
        }
      else
        {
          sprintf (name, "Parcellation %d, %d, %d", r, g, b);
        }
      
      /* set its other data. set the color; if we found a
         structure index from the LUT, it will use that,
         otherwise it will color it as a free label with
         the given colors (which really has the same
         effect, just doesn't give it a valid structure
         index. */
      labl_set_info (new_index, name, structure, 1, r, g, b);
    }
      }
  }
    }
  
  free (done);

  /* show the label label in the interface. */
  if (g_interp)
    Tcl_Eval (g_interp, "ShowLabel kLabel_Label 1");

  return (ERROR_NONE);
}

int labl_export_annotation (char *fname) 
{
  int vno;
  int label_index;
  int color;
  LABL_LABEL* label;
  int label_vno;
  int r = 0;
  int g = 0;
  int b = 0;
  
  if (NULL == fname)
    return (ERROR_BADPARM);
  
  for (vno = 0; vno < mris->nvertices; vno++)
    mris->vertices[vno].annotation = 0;
  
  /* for each label.. */
  for (label_index = 0; label_index < labl_num_labels; label_index++)
    {
      
      label = &(labl_labels[label_index]);

      /* if this is not a free label... */
      if ( LABL_TYPE_FREE != label->structure)
  {
    
    /* make the composed color int for this label. */
    LABL_PACK_ANNOT (label->r, label->g, label->b, color);
#if 0
    r = label->r & 0xff;
    g = (label->g & 0xff) << 8;
    b = (label->b & 0xff) << 16;
    color = r | g | b;
#endif     
 
    /* for every vertex in the label... */
    for (label_vno = 0; label_vno < label->label->n_points; label_vno++)
      {
        /* set the annotation value. */
        mris->vertices[label->label->lv[label_vno].vno].annotation = color;
      }
    
    if (labl_debug)
      {
        printf( "saved label %d with %d vertices, color %d %d %d "
          "anot value %d\n", label_index, label->label->n_points,
          r, g, b,color );
      }
  }
    }
  
  /* write out the annotation. */
  MRISwriteAnnotation (mris, fname);
  
  return (ERROR_NONE);
}

int labl_new_from_marked_vertices () 
{
  LABEL* label = NULL;
  int num_marked_verts;
  int vno;
  int label_vno;
  VERTEX* v = NULL;
  int new_index;
  
  /* count the number of marked vertices. */
  num_marked_verts = 0;
  for (vno = 0 ; vno < mris->nvertices ; vno++)
    if (mris->vertices[vno].marked)
      num_marked_verts++;
  
  /* if we didn't get any, return. */
  if (0 == num_marked_verts) 
    {
      fprintf (stderr, "no marked vertices...\n");
      return (ERROR_NONE);
    }
  
  /* allocate a label. */
  label = LabelAlloc(num_marked_verts, NULL, NULL);
  
  /* for every vertex, if it's marked, save its vertex coords,
     index. don't fill the value of the current overlay, as that
     should only be done when the label is actually written to
     file. */
  label_vno = 0;
  for (vno = 0; vno < mris->nvertices; vno++)
    {
      v = &mris->vertices[vno];
      if (v->marked)
  {
    label->lv[label_vno].x = v->x;
    label->lv[label_vno].y = v->y;
    label->lv[label_vno].z = v->z;
    label->lv[label_vno].vno = vno;
    label_vno++;
  }
    }
  label->n_points = num_marked_verts;
  
  /* convert to original positions. */
  if (!origsurfloaded)
    read_orig_vertex_coordinates(orfname);
  LabelToOriginal (label, mris);
  
  /* add this label to our list. */
  labl_add (label, &new_index);
  
  /* select this label */
  labl_select (new_index);
  
  /* clear the marked verts. */
  clear_all_vertex_marks ();
  
  surface_compiled = 0 ;
  
  return (ERROR_NONE);
}

int labl_mark_vertices (int index)
{
  if (index < 0 || index >= labl_num_labels)
    return (ERROR_BADPARM);
  
  /* mark the verts. */
  LabelMark (labl_labels[index].label, mris);

  return (ERROR_NONE);
}

int labl_select (int index) 
{
  char tcl_command[NAME_LENGTH + 50];
  int old_selected;
  
  /* mark this label as selected. */
  old_selected = labl_selected_label;
  labl_selected_label = index;
  
  /* if something changed... */
  if (old_selected != labl_selected_label)
    {
      /* if something was selected, send the select cpmmand and the
   update command to the tcl label list with this label's
   information. */
      if ((index >= 0 && index < labl_num_labels) && g_interp)
  {
    sprintf (tcl_command, "LblLst_SelectLabel %d", index);
    Tcl_Eval (g_interp, tcl_command);
    
    labl_send_info (index);
  }
      
      /* redraw. */
      redraw ();
    }
  
  return (ERROR_NONE);
}

int labl_set_name_from_table (int index)
{
  char name[NAME_LENGTH];
  LABL_LABEL* label;
  int clut_err;

  if (index < 0 || index >= labl_num_labels)
    return (ERROR_BADPARM);
  
  if (NULL == labl_table)
    return (ERROR_NONE);

  /* get the name of this structure. if we get it, set the name. pass
     the other data in as the same. */
  label = &(labl_labels[index]);
  clut_err = CLUT_GetLabel (labl_table, label->structure, name);
  if (CLUT_tErr_NoErr == clut_err)
    labl_set_info (index, name, label->structure, label->visible,
       label->r, label->g, label->b);
  
  return (ERROR_NONE);
}

int labl_set_info (int index, char* name, int structure, int visible,
       int r, int g, int b)
{
  CLUT_tErr clut_err = CLUT_tErr_NoErr;
  xColor3n color;
  
  if (index < 0 || index >= labl_num_labels)
    return (ERROR_BADPARM);
  if (NULL == name)
    return (ERROR_BADPARM);
  if (0 != visible && 1 != visible)
    return (ERROR_BADPARM);

  /* set the info in this label. */
  strncpy (labl_labels[index].name, name, NAME_LENGTH);
  labl_labels[index].structure = structure;
  labl_labels[index].visible = visible;

  /* if we have a table, and the structure is not free, get the color
     from the table. otherwise, use the color that they gave us. */
  if (labl_table && 
      LABL_TYPE_FREE != labl_labels[index].structure)
    {
      clut_err = CLUT_GetColorInt (labl_table, structure, &color);
      if (CLUT_tErr_NoErr == clut_err)
  {
    labl_labels[index].r = color.mnRed;
    labl_labels[index].g = color.mnGreen;
    labl_labels[index].b = color.mnBlue;
  }
    } else {
      labl_labels[index].r = r;
      labl_labels[index].g = g;
      labl_labels[index].b = b;
    }

  /* send the label info to tcl */
  labl_send_info (index);

  return (ERROR_NONE);
}

int labl_set_color (int index, int r, int g, int b)
{
  if (index < 0 || index >= labl_num_labels)
    return (ERROR_BADPARM);
  if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255)
    return (ERROR_BADPARM);
  
  /* set the color */
  labl_labels[index].r = r;
  labl_labels[index].g = g;
  labl_labels[index].b = b;

  /* send the label info to tcl */
  labl_send_info (index);

  return (ERROR_NONE);
}

int labl_send_info (int index)
{
  char tcl_command[NAME_LENGTH + 50];

  if (index < 0 || index >= labl_num_labels)
    return (ERROR_BADPARM);

  /* send the update command to the tcl label list with this label's
     information. */
  if (g_interp)
    {
      sprintf (tcl_command, "LblLst_UpdateInfo %d \"%s\" %d %d %d %d %d", 
         index, labl_labels[index].name, 
         labl_labels[index].structure, labl_labels[index].visible,
         labl_labels[index].r, labl_labels[index].g, 
         labl_labels[index].b );
      Tcl_Eval (g_interp, tcl_command);
    }
  
  return (ERROR_NONE);
}

int labl_add (LABEL* label, int* new_index)
{
  int index;
  float min_x, max_x;
  float min_y, max_y;
  float min_z, max_z;
  int label_vno;
  char tcl_command[NAME_LENGTH + 50];
  
  if (NULL == label)
    return (ERROR_BADPARM);
  
  /* make sure we can add one. */ 
  if (labl_num_labels >= LABL_MAX_LABELS)
    return (ERROR_NO_MEMORY);
  
  /* add a new node at the end of the list. */
  index = labl_num_labels;
  labl_num_labels++;
  
  /* copy the label in and init the other data. make it a free label
     by default, with the default color. */
  labl_labels[index].label = label;
  labl_labels[index].structure = LABL_TYPE_FREE;
  labl_labels[index].r = LABL_DEFAULT_COLOR_R;
  labl_labels[index].g = LABL_DEFAULT_COLOR_G;
  labl_labels[index].b = LABL_DEFAULT_COLOR_B;
  labl_labels[index].visible = 1;
  
  /* set the name using a global counter and increment the counter. */
  labl_num_labels_created++;
  sprintf (labl_labels[index].name, "Label %d", labl_num_labels_created);
  
  /* go through the label and find the bounds. */
  min_x = min_y = min_z = 500;
  max_x = max_y = max_z = -500;
  for (label_vno = 0; label_vno < label->n_points; label_vno++)
    {
      if (label->lv[label_vno].x < min_x)
  min_x = label->lv[label_vno].x;
      if (label->lv[label_vno].y < min_y)
  min_y = label->lv[label_vno].y;
      if (label->lv[label_vno].z < min_z)
  min_z = label->lv[label_vno].z;
      if (label->lv[label_vno].x > max_x)
  max_x = label->lv[label_vno].x;
      if (label->lv[label_vno].y > max_y)
  max_y = label->lv[label_vno].y;
      if (label->lv[label_vno].z > max_z)
  max_z = label->lv[label_vno].z;
    }
  
  /* set the bounds in the label, modifying it by the fudge value. */
  labl_labels[index].min_x = min_x - LABL_FUDGE;
  labl_labels[index].min_y = min_y - LABL_FUDGE;
  labl_labels[index].min_z = min_z - LABL_FUDGE;
  labl_labels[index].max_x = max_x + LABL_FUDGE;
  labl_labels[index].max_y = max_y + LABL_FUDGE;
  labl_labels[index].max_z = max_z + LABL_FUDGE;
  
  /* invalidate the cache. */
  labl_cache_is_current = 0;
  
  /* find the border. */
  labl_find_and_set_border (index);
  
  if (g_interp)
    {
      /* notify tcl of the new label. */
      sprintf (tcl_command, "LblLst_AddLabel \"%s\"", labl_labels[index].name);
      Tcl_Eval (g_interp, tcl_command);
      
      /* enable our label menu items. */
      enable_menu_set (MENUSET_LABEL_LOADED, 1);
    }
  
  /* if they want the new index, return it. */
  if (NULL != new_index)
    *new_index = index;
  
  return (ERROR_NONE);
}

int labl_remove (int index) 
{
  int next;
  char tcl_command[NAME_LENGTH];
  
  if (index < 0 || index >= labl_num_labels)
    return (ERROR_BADPARM);
  
  /* clear the border flags first. */
  labl_clear_border (index);
  
  /* free the label here. */
  LabelFree (&labl_labels[index].label);
  
  /* for every label above it, copy it one slot down. */
  next = index + 1;
  while (next < labl_num_labels)
    {
      labl_labels[next-1] = labl_labels[next];
      next++;
    }

  /* decrement the number of labels. */
  labl_num_labels--;

  /* send the update command to the tcl label list with this label's
     information. */
  if (g_interp)
    {
      sprintf (tcl_command, "LblLst_RemoveLabel %d", index );
      Tcl_Eval (g_interp, tcl_command);
    }

  /* if this was our selected label, select nothing. */
  if (labl_selected_label == index)
    labl_select (-1);

  /* if our selection was above label selected, decrement it. */
  else if (labl_selected_label > index)
    labl_selected_label--;

  /* invalidate the cache. */
  labl_cache_is_current = 0;

  return (ERROR_NONE);
}

int labl_remove_all () {

  int labels;
  
  /* delete all the labels. */
  labels = labl_num_labels;
  while (labels > 0) 
    {
      labl_remove (0);
      labels--;
    }

  return (ERROR_NONE);
}

int labl_find_label_by_vno (int vno, int* index)
{
  int label_index;
  int label_vno;
  float x, y, z;
  LABEL* label;
  
  /* update the cache if not done so. */
  if(!labl_cache_is_current)
    {
      labl_update_label_cache();
    }

  /* if it updated properly, use it to get our value. */
  if (labl_top_label && labl_cache_is_current)
    {
      *index = labl_top_label[vno];
      return(ERROR_NONE);
    }

  /* get the location of this vertex. */
  x = mris->vertices[vno].origx;
  y = mris->vertices[vno].origy;
  z = mris->vertices[vno].origz;

  /* go through each label looking for this vno... */
  for (label_index = 0; label_index < labl_num_labels; label_index++)
    {
      /* check the bounds here. */
      if (x >= labl_labels[label_index].min_x &&
    x <= labl_labels[label_index].max_x &&
    y >= labl_labels[label_index].min_y &&
    y <= labl_labels[label_index].max_y &&
    z >= labl_labels[label_index].min_z &&
    z <= labl_labels[label_index].max_z)
  {
    label = labl_labels[label_index].label;
   
    /* if this label contains this vno, return it. */
    for (label_vno = 0; label_vno < label->n_points; label_vno++)
      {
        if (label->lv[label_vno].vno == vno)
    {
      *index = label_index;
      
      if (labl_debug)
        {
          printf( "Label %d\n", label_index );
          printf( "\tName: %s, %d vertices\n", 
            labl_labels[label_index].name, 
            labl_labels[label_index].label->n_points );
          printf( "\tStructure: %d Color: %d %d %d Visible: %d\n", 
            labl_labels[label_index].structure, 
            labl_labels[label_index].r, 
            labl_labels[label_index].g, 
            labl_labels[label_index].b, 
            labl_labels[label_index].visible );
        }
      
      return (ERROR_NONE);
    }
      }
  }
    }
  
  /* get here, there's no label. */
  *index = -1;

  return (ERROR_NONE);
} 

int labl_select_label_by_vno (int vno)
{
  int label_index = -1;

  if (vno < 0 || vno >= mris->nvertices)
    return (ERROR_BADPARM);
  
  /* try and find a label. if found, select it. */
  labl_debug = 1;
  labl_find_label_by_vno (vno, &label_index);
  labl_debug = 0;
  labl_select (label_index);

  return (ERROR_NONE);
} 

int labl_apply_color_to_vertex (int vno, GLubyte* r, GLubyte* g, GLubyte* b )
{
  int label_index = -1;

  if (vno < 0 || vno >= mris->nvertices)
    return (ERROR_BADPARM);
  
  /* if our display flag is off, do nothing. */
  if( !labl_draw_flag ) {
    return (ERROR_NONE);
  }

  /* try and find a label. if found... */
  labl_find_label_by_vno (vno, &label_index);
  if (-1 != label_index)
    {
      /* if this is the selected label and this is a border of width 1
         or 2, make it yellow. */
      if (labl_selected_label == label_index && labl_is_border[vno])
  {
    *r = 255;
    *g = 255;
    *b = 0;
  }
      /* else if this label is visible... */
      else if (labl_labels[label_index].visible)
  {
    /* color it in the given drawing style. */
    switch (labl_draw_style)
      {
      case LABL_STYLE_OPAQUE:
        *r = labl_labels[label_index].r;
        *g = labl_labels[label_index].g;
        *b = labl_labels[label_index].b;
        break;
      case LABL_STYLE_OUTLINE:
        /* if this is a border of width 1, color it the color of the
     label. */
        if (1 == labl_is_border[vno])
    {
      *r = labl_labels[label_index].r;
      *g = labl_labels[label_index].g;
      *b = labl_labels[label_index].b;
    }
        break;
      default:
        ;
      }
  }
    }
  
  return (ERROR_NONE);
}

int labl_print_list ()
{
  int label_index;
  LABL_LABEL* label;

  printf ("Num labels: %d (%d) Num strucutres: %d\n", 
    labl_num_labels, labl_selected_label, labl_num_structures );
  for (label_index = 0; label_index < labl_num_labels; label_index++)
    {
      label = &(labl_labels[label_index]);
      
      printf ("Label %d\n", label_index );
      printf ("\tName: %s, %d vertices\n", 
        label->name, label->label->n_points );
      printf ("\tStructure: %d Color: %d %d %d Visible: %d\n", 
        label->structure, label->r, label->g, label->b, label->visible );
      printf ("\tBounds: x %f %f y %f %f z %f %f\n",
        label->min_x, label->max_x, 
        label->min_y, label->max_y,
        label->min_z, label->max_z);
    }

  return (ERROR_NONE);
}

int labl_print_table ()
{
  int structure_index;
  xColor3n color;
  char structure_label[CLUT_knLabelLen];

  printf( "Num strucutres: %d\n", labl_num_structures );
  for (structure_index = 0; structure_index < labl_num_structures; 
       structure_index++)
    {
      CLUT_GetColorInt (labl_table, structure_index, &color);
      CLUT_GetLabel (labl_table, structure_index, structure_label);
      
      printf( "Structure %d: %s r %d g %d b %d", 
        structure_index, structure_label, 
        color.mnRed, color.mnGreen, color.mnBlue );
    }

  return (ERROR_NONE);
}

/* ---------------------------------------------------------------------- */

/* ------------------------------------------------------ fill boundaries */
static int
fbnd_set_marks(FBND_BOUNDARY *b, MRI_SURFACE *mris, int mark)
{
  int i, vno ;

  for (i = 0 ; i < b->num_vertices ; i++)
  {
    vno = b->vertices[i] ;
    if (vno < 0 || vno >= mris->nvertices)
      continue ;
    mris->vertices[vno].marked = mark ;
  }
  return(NO_ERROR) ;

}

FBND_BOUNDARY *
fbnd_copy(FBND_BOUNDARY *bsrc, FBND_BOUNDARY *bdst)
{
  int   vno ;

  if (!bdst)
  {
    bdst = (FBND_BOUNDARY *)calloc(1, sizeof(FBND_BOUNDARY)) ;
    bdst->vertices = (int *)calloc(bsrc->num_vertices, sizeof(int)) ;
    if (!bdst->vertices)
      ErrorExit(ERROR_NOMEMORY, "%s: could not copy boundary with %d vertices",
                Progname, bdst->num_vertices) ;
  }

  bdst->num_vertices = bsrc->num_vertices ;
  bdst->min_x = bsrc->min_x ; bdst->max_x = bsrc->max_x ;
  bdst->min_y = bsrc->min_y ; bdst->max_y = bsrc->max_y ;
  bdst->min_z = bsrc->min_z ; bdst->max_z = bsrc->max_z ;
  for (vno = 0 ; vno < bdst->num_vertices ; vno++)
    bdst->vertices[vno] = bsrc->vertices[vno] ;
  return(bdst) ;
}

int fbnd_initialize ()
{
  int boundary;

  /* clear the boundary array. */
  for (boundary = 0; boundary < FBND_MAX_BOUNDARIES; boundary++)
    {
      fbnd_boundaries[boundary].num_vertices = 0;
      fbnd_boundaries[boundary].vertices = NULL;
    }
  
  /* default values. */
  fbnd_selected_boundary = FBND_NONE_SELECTED;
  fbnd_num_boundaries = 0;

  return (ERROR_NONE);
}

int fbnd_new_line_from_marked_vertices ()
{
  int marked_index;
  int src_vno, dest_vno;
  int vno;
  char* check;
  float* dist;
  int* pred;
  char done;
  VERTEX* v;
  VERTEX* u;
  float closest_dist;
  int closest_vno;
  int neighbor;
  int neighbor_vno;
  float dist_uv;
  int* path;
  int path_vno;
  int num_path = 0;
  int num_checked;
  float vu_x, vu_y, vu_z;
  float srcdst_x, srcdst_y, srcdst_z;
  int skipped_verts;

  dist = (float*) calloc (mris->nvertices, sizeof(float));
  pred = (int*) calloc (mris->nvertices, sizeof(int));
  check = (char*) calloc (mris->nvertices, sizeof(char));
  path = (int*) calloc (mris->nvertices, sizeof(int));
  num_path = 0;
  num_checked = 0;
  skipped_verts = 0;
  
  printf ("surfer: making line");
  cncl_start_listening ();
  for (marked_index = 0; marked_index < nmarked-1; marked_index++)
    {
      if (cncl_user_canceled())
  break;

      src_vno = marked[marked_index];
      dest_vno = marked[marked_index+1];
      
      /* clear everything */
      for (vno = 0; vno < mris->nvertices; vno++)
  {
    dist[vno] = 999999;
    pred[vno] = -1;
    check[vno] = FALSE;
  }
      
      /* calc the vector from src to dst. */
      srcdst_x = mris->vertices[dest_vno].x - mris->vertices[src_vno].x;
      srcdst_y = mris->vertices[dest_vno].y - mris->vertices[src_vno].y;
      srcdst_z = mris->vertices[dest_vno].z - mris->vertices[src_vno].z;
      
      /* pull the src vertex in. */
      dist[src_vno] = 0;
      pred[src_vno] = vno;
      check[src_vno] = TRUE;
      
      done = FALSE;
      while (!done)
  {
    if (cncl_user_canceled())
      break;

    /* find the vertex with the shortest edge. */
    closest_dist = 999999;
    closest_vno = -1;
    for (vno = 0; vno < mris->nvertices; vno++)
      if (check[vno])
        if (dist[vno] < closest_dist) 
    {
      closest_dist = dist[vno];
      closest_vno = vno;
    }
    v = &(mris->vertices[closest_vno]);
    check[closest_vno] = FALSE;
    
    /* if this is the dest node, we're done. */
    if (closest_vno == dest_vno)
      {
        done = TRUE;
      } 
    else
      {
        /* relax its neighbors. */
        for (neighbor = 0; neighbor < v->vnum; neighbor++)
    {
      neighbor_vno = v->v[neighbor];
      u = &(mris->vertices[neighbor_vno]);
      
      /* calc the vector from u to v. */
      vu_x = u->x - v->x;
      vu_y = u->y - v->y;
      vu_z = u->z - v->z;
      
      /* calc the dot product between srcdest vector and
                     uv vector. if it's < 0, this neighbor is in the
                     opposite direction of dest, and we can skip it. */
      if ( (vu_x * srcdst_x) + 
           (vu_y * srcdst_y) +
           (vu_z * srcdst_z)  < 0 )
        {
          skipped_verts++;
          continue;
        }
      
      /* recalc the weight. */
      dist_uv = ((v->x - u->x) * (v->x - u->x)) +
        ((v->y - u->y) * (v->y - u->y)) +
        ((v->z - u->z) * (v->z - u->z));
      
      /* if this is a new shortest path, update the predecessor,
         weight, and add it to the list of ones to check next. */
      if (dist_uv + dist[closest_vno] < dist[neighbor_vno])
        {
          pred[neighbor_vno] = closest_vno;
          dist[neighbor_vno] = dist_uv + dist[closest_vno];
          check[neighbor_vno] = TRUE;
        }
    }
      }
    num_checked++;
    if ((num_checked % 100) == 0)
      {
        printf (".");
        fflush (stdout);
      }
  }

      /* add the predecessors from the dest to the src to the path. */
      path_vno = dest_vno;
      path[num_path++] = dest_vno;
      while (pred[path_vno] != src_vno)
  {
    path[num_path++] = pred[path_vno];
    path_vno = pred[path_vno];
  }
    }
  cncl_stop_listening ();
  
  printf (" done\n");
  
  fbnd_add (num_path, path, NULL);
  
  free (dist);
  free (pred);
  free (check);
  free (path);

  return (ERROR_NONE);
}

int fbnd_remove_selected_boundary () 
{
  if (fbnd_selected_boundary >= 0 && 
      fbnd_selected_boundary < fbnd_num_boundaries)
    fbnd_remove (fbnd_selected_boundary);

  return (ERROR_NONE);
}

int fbnd_add (int num_vertices, int* vertices, int* new_index)
{
  int index;
  float min_x, max_x;
  float min_y, max_y;
  float min_z, max_z;
  int boundary;
  VERTEX* v;

  if (num_vertices <= 0)
    return (ERROR_BADPARM);
  if (NULL == vertices)
    return (ERROR_BADPARM);

  /* make sure we can add one. */ 
  if (fbnd_num_boundaries >= FBND_MAX_BOUNDARIES)
    return (ERROR_NO_MEMORY);

  /* add a new one at the end of the array. */
  index = fbnd_num_boundaries;
  fbnd_num_boundaries++;

  /* allocate the vertices array and copy everything in. */
  fbnd_boundaries[index].num_vertices = num_vertices;
  fbnd_boundaries[index].vertices = (int*) calloc (num_vertices, sizeof(int));
  if (NULL == fbnd_boundaries[index].vertices)
    {
      printf ("fbnd_add: calloc failed with %d elements\n", num_vertices);
      return (ERROR_NO_MEMORY);
    }
  memcpy (fbnd_boundaries[index].vertices, vertices, 
    num_vertices * sizeof(int));

  /* go through the label and find the bounds. */
  min_x = min_y = min_z = 500;
  max_x = max_y = max_z = -500;
  for (boundary = 0; 
       boundary < fbnd_boundaries[index].num_vertices; 
       boundary++)
    {
      v = &(mris->vertices[ fbnd_boundaries[index].vertices[boundary] ]);

      if (v->x < min_x)
  min_x = v->x;
      if (v->y < min_y)
  min_y = v->y;
      if (v->z < min_z)
  min_z = v->z;
      if (v->x > max_x)
  max_x = v->x;
      if (v->y > max_y)
  max_y = v->y;
      if (v->z > max_z)
  max_z = v->z;
    }

  /* set the bounds in the label, modifying it by the fudge value. */
  fbnd_boundaries[index].min_x = min_x - FBND_FUDGE;
  fbnd_boundaries[index].min_y = min_y - FBND_FUDGE;
  fbnd_boundaries[index].min_z = min_z - FBND_FUDGE;
  fbnd_boundaries[index].max_x = max_x + FBND_FUDGE;
  fbnd_boundaries[index].max_y = max_y + FBND_FUDGE;
  fbnd_boundaries[index].max_z = max_z + FBND_FUDGE;

  /* update the boundaries flags */
  fbnd_update_surface_boundaries ();

  /* return the new index if they want it. */
  if (NULL != new_index)
    *new_index = index;

  return (ERROR_NONE);
}

int fbnd_remove (int index)
{
  int next;

  if (index < 0 || index >= fbnd_num_boundaries)
    return (ERROR_BADPARM);

  /* free the vertices array. */
  free (fbnd_boundaries[index].vertices);

  /* bump everything above this index down one. */
  next = index + 1;
  while (next < fbnd_num_boundaries)
    {
      fbnd_boundaries[next-1] = fbnd_boundaries[next];
      next++;
    }

  /* decrement the number of boundaries. */
  fbnd_num_boundaries--;

  /* update the boundaries flags */
  fbnd_update_surface_boundaries ();

  /* if this was our selected boundary, select nothing. */
  if (fbnd_selected_boundary == index)
    fbnd_select (-1);

  /* if our selection was above boundary selected, decrement it. */
  else if (fbnd_selected_boundary > index)
    fbnd_selected_boundary--;


  return (ERROR_NONE);
}

int fbnd_select (int index)
{
  int old_selected;

  /* select this boundary. */
  old_selected = fbnd_selected_boundary;
  fbnd_selected_boundary = index;
  
  if (old_selected != fbnd_selected_boundary)
    redraw();

  return (ERROR_NONE);
}

int fbnd_update_surface_boundaries ()
{
  int vno;
  int boundary;
  int boundary_vno;

  /* make our boundary flag array if we haven't already. */
  if (NULL == fbnd_is_boundary)
    {
      fbnd_is_boundary = (char*) calloc (mris->nvertices, sizeof(char));
      if (NULL == fbnd_is_boundary)
  return (ERROR_NO_MEMORY);
    }

  /* turn off all the boundary flags. */
  for (vno = 0; vno < mris->nvertices; vno++ )
    fbnd_is_boundary[vno] = FALSE;

  /* for every boundary... */
  for (boundary = 0; boundary < fbnd_num_boundaries; boundary++)
    {
      /* for every vertex, mark that vertex in the array as a
         boundary. */
      for (boundary_vno = 0; 
     boundary_vno < fbnd_boundaries[boundary].num_vertices; 
     boundary_vno++ )
  {
    fbnd_is_boundary[fbnd_boundaries[boundary].vertices[boundary_vno]] = TRUE;
  }
    }

  return (ERROR_NONE);
}

char fbnd_is_vertex_on_boundary (int vno)
{
  if (vno < 0 || vno >= mris->nvertices)
    return (FALSE);
  if (NULL == fbnd_is_boundary)
    return (FALSE);

  /* return the value of the boundary flag here. */
  return (fbnd_is_boundary[vno]);
}

int fbnd_select_boundary_by_vno (int vno)
{
  int boundary;
  int boundary_vno;
  float x, y, z;
  VERTEX *u;
  VERTEX *v;
  float dist_uv;

  if (vno < 0 || vno >= mris->nvertices)
    return (FALSE);
  if (NULL == fbnd_is_boundary)
    return (FALSE);

  v = &(mris->vertices[vno]);

  x = v->x;
  y = v->y;
  z = v->z;

  for (boundary = 0; boundary < fbnd_num_boundaries; boundary++)
    {
      if (x >= fbnd_boundaries[boundary].min_x &&
    x <= fbnd_boundaries[boundary].max_x &&
    y >= fbnd_boundaries[boundary].min_y &&
    y <= fbnd_boundaries[boundary].max_y &&
    z >= fbnd_boundaries[boundary].min_z &&
    z <= fbnd_boundaries[boundary].max_z)
  {
    for (boundary_vno = 0; 
         boundary_vno < fbnd_boundaries[boundary].num_vertices;
         boundary_vno++)
      {  
        u = &(mris->vertices[fbnd_boundaries[boundary].vertices[boundary_vno]]);
        
        dist_uv = ((v->x - u->x) * (v->x - u->x)) +
    ((v->y - u->y) * (v->y - u->y)) +
    ((v->z - u->z) * (v->z - u->z));

        if (dist_uv < 25)
    {
      fbnd_select (boundary);
      return (ERROR_NONE);
    }
      }
  }
    }

  fbnd_select (-1);
  return (ERROR_NONE);
}

int fbnd_apply_color_to_vertex (int vno, GLubyte* r, GLubyte* g, GLubyte* b )
{
  float x, y, z;
  char selected;
  int boundary_vno;

  if (vno < 0 || vno >= mris->nvertices)
    return (ERROR_BADPARM);
  if (NULL == r || NULL == g || NULL == b)
    return (ERROR_BADPARM);

  /* if this is a boundary.. */
  if (fbnd_is_vertex_on_boundary(vno))
    {
      
      /* get the location of this vertex. */
      x = mris->vertices[vno].x;
      y = mris->vertices[vno].y;
      z = mris->vertices[vno].z;
      
      /* see if it's in the selected bound cube, then in the selectd
         boundary vertex list. */
      selected = FALSE;
      if (x >= fbnd_boundaries[fbnd_selected_boundary].min_x &&
    x <= fbnd_boundaries[fbnd_selected_boundary].max_x &&
    y >= fbnd_boundaries[fbnd_selected_boundary].min_y &&
    y <= fbnd_boundaries[fbnd_selected_boundary].max_y &&
    z >= fbnd_boundaries[fbnd_selected_boundary].min_z &&
    z <= fbnd_boundaries[fbnd_selected_boundary].max_z)
  for (boundary_vno = 0; 
       boundary_vno < fbnd_boundaries[fbnd_selected_boundary].num_vertices;
       boundary_vno++)
    if (vno == 
        fbnd_boundaries[fbnd_selected_boundary].vertices[boundary_vno])
      selected = TRUE;
      
      if (selected)
  {
    /* if it's selected, mark it yellow. */
    *r = 255;
    *g = 255;
    *b = 0;
  }
      else
  {
    /* else just color it red. */
    *r = 255;
    *g = 0;
    *b = 0;
  }
    }

  return (ERROR_NONE);
}

/* ---------------------------------------------------------------------- */

/* ------------------------------------------------------- floodfill mark */

int fill_flood_from_seed (int seed_vno, FILL_PARAMETERS* params)
{
  char* filled;
  int num_filled_this_iter;
  int num_filled;
  int iter;
  int min_vno, max_vno, step_vno;
  int vno;
  int label = 0;
  int neighbor_index;
  int neighbor_vno;
  int neighbor_label;
  VERTEX* v;
  VERTEX* neighbor_v;
  float fvalue;
  float seed_curv = 0;

  if (seed_vno < 0 || seed_vno >= mris->nvertices)
    return (ERROR_BADPARM);
  if (NULL == params)
    return (ERROR_BADPARM);

  /* init filled array. */
  filled = (char*) calloc (mris->nvertices, sizeof(char));

  /* start with the seed filled.*/
  filled[seed_vno] = TRUE;

  /* find seed values for some conditions. */
  if (params->dont_cross_label)
    labl_find_label_by_vno (seed_vno, &label);
  if (params->dont_cross_cmid)
    seed_curv = mris->vertices[seed_vno].curv;

  /* while we're still filling stuff in a pass... */
  num_filled_this_iter = 1;
  num_filled = 0;
  iter = 0;
  while (num_filled_this_iter > 0)
  {
    num_filled_this_iter = 0;

    /* switch between iterating forward and backwards. */
    if ((iter%2)==0)
    {
      min_vno = 0;
      max_vno = mris->nvertices-1;
      step_vno = 1;
    } else
    {
      min_vno = mris->nvertices-1;
      max_vno = 0;
      step_vno = -1;
    }

    /* for each vertex, if it's filled, check its neighbors. for the
       rules that are up-to-and-including, make the check on this
       vertex. for the rules that are up-to-and-not-including, check
       on the neighbor. */
    for (vno = min_vno; vno != max_vno; vno += step_vno)
    {
      if (filled[vno])
      {
  /* check the neighbors... */
        v = &mris->vertices[vno];

  /* if we're not crossing boundaries, check if this is a
     boundary. if so, move on. */
  if (params->dont_cross_boundary && 
      fbnd_is_vertex_on_boundary (vno))
    {
      continue;
    }

  /* if we're not crossing the cmid, see if the cmid at this
     vertex is on the other side of the cmid as the seed
     point. if so, move on. */
  if (params->dont_cross_cmid &&
      ((seed_curv <= cmid && v->curv > cmid) ||
       (seed_curv >= cmid && v->curv < cmid)))
    {
      continue;
    }
  
        for (neighbor_index = 0; neighbor_index < v->vnum; neighbor_index++)
    {
      neighbor_vno = v->v[neighbor_index];
      neighbor_v = &mris->vertices[neighbor_vno] ;

      /* if the neighbor is filled, move on. */
      if (filled[neighbor_vno])
    continue;

      /* if we're not crossing labels, check if the label at
               this vertex is the same as the one at the seed. if not,
               move on. */
      if (params->dont_cross_label)
        {
    labl_find_label_by_vno (neighbor_vno, &neighbor_label);
    if (neighbor_label != label)
      {
        continue;
      }
        }

      /* if we're not crossing the fthresh, make sure this point
               is above it. if not, move on. */
      if (params->dont_cross_fthresh)
        {
    sclv_get_value (neighbor_v, sclv_current_field, &fvalue);
    if (fabs(fvalue) < fthresh)
      {
        continue;
      }
        }
      
      /* mark this vertex as filled. */
      filled[neighbor_vno] = TRUE;
      num_filled_this_iter++;
      num_filled++;
    }
      }
    }

    iter++;
  }

  /* mark all filled vertices. */
  for (vno = 0; vno < mris->nvertices; vno++ )
    if (filled[vno])
      mris->vertices[vno].marked = TRUE;
  
  free (filled);

  /* make a new label from the marked vertices. */
  labl_new_from_marked_vertices ();

  return (ERROR_NONE);
}

/* ---------------------------------------------------------------------- */

/* end rkt */

static int
is_val_file(char *fname)
{
  char   *dot ;
  
  dot = strrchr(fname, '.') ;
  if (!dot)
    return(0) ;
  return (!stricmp(dot+1, "w"));
}

#include "cdflib.h"
#include "sig.h"
    
static void
f_to_p(int numer_dof, int denom_dof)
{
  int    vno ;
  VERTEX *v ;
  float sig ;

  surface_compiled = 0 ;
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    sig = sigf(v->val, numer_dof, denom_dof) ;
    if (v->val > 0)
      v->stat = -log10(sig) ;
    else
      v->stat = log10(sig) ;
  }
}

static void
t_to_p(int dof)
{
  int    vno ;
  VERTEX *v ;
  float  sig ;

  surface_compiled = 0 ;
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    sig = sigt(v->val, dof) ;
    if (v->val > 0)
      v->stat = -log10(sig) ;
    else
      v->stat = log10(sig) ;
  }
}

static void
f_to_t(void)
{
  int    vno ;
  VERTEX *v ;
  float  f ;

  surface_compiled = 0 ;
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;
    f = v->val ;
    v->val = sqrt(abs(f)) ;
    if (f < 0)
      v->val = -v->val ;
  }
}

static void
label_to_stat(void)
{
  VERTEX *v ;
  LABEL  *area ;
  int    n, field = SCLV_VALSTAT ;
  float  mn, mx, f ;
  char                  cmd[STRLEN];

  if (labl_selected_label == LABL_NONE_SELECTED)
  {
    fprintf(stderr, "no label currently selected....\n") ;
    return ;
  }

  enable_menu_set (MENUSET_OVERLAY_LOADED, 1);
  area = labl_labels[labl_selected_label].label ;
  mn = mx = 0.0 ;
  for (n = 0  ; n < area->n_points ; n++)
  {
    v = &mris->vertices[area->lv[n].vno] ;
    f = area->lv[n].stat ;
    if (n == 0)
      mn = mx = f ;
    sclv_set_value(v, field, f) ;
    /*    v->stat = f ;*/
    if (f > mx)
      mx = f ;
    if (f < mn)
      mn = f ;
  }
  if (Gdiag & DIAG_SHOW && DIAG_VERBOSE_ON)
    printf("min = %f, max = %f\n", mn, mx) ;
  set_value_label_name(labl_labels[labl_selected_label].name, field) ;
  sclv_field_info[field].min_value = mn;
  sclv_field_info[field].max_value = mx;
  sclv_field_info[field].cur_timepoint = 0;
  sclv_field_info[field].cur_condition = 0;
  sclv_field_info[field].num_timepoints = 1;
  sclv_field_info[field].num_conditions = 1;
  /* calc the frquencies */
  sclv_calc_frequencies (field);
  /* request a redraw. turn on the overlay flag and select this value set */
  vertex_array_dirty = 1 ;
  overlayflag = TRUE;
  sclv_set_current_field (field);
  
  /* set the field name to the name of the file loaded */
  if (NULL != g_interp)
  {
    sprintf (cmd, "UpdateValueLabelName %d \"%s\"", field, labl_labels[labl_selected_label].name);
    Tcl_Eval (g_interp, cmd);
    sprintf (cmd, "ShowValueLabel %d 1", field);
    Tcl_Eval (g_interp, cmd);
    sprintf (cmd, "UpdateLinkedVarGroup view");
    Tcl_Eval (g_interp, cmd);
    sprintf (cmd, "UpdateLinkedVarGroup overlay");
    Tcl_Eval (g_interp, cmd);
  }
}


#include "stc.h"

static double get_dipole_value(int dipno, int ilat, int plot_type, STC *stc);
static STC  *stc ;


static void
read_soltimecourse(char *fname)
{

  surface_compiled = 0 ;
  stc = StcRead(fname) ;
  if (!stc)
    return ;
  if (stc->nvertices < stc->m_vals->rows)
    sol_nperdip = 3 ;
  else
    sol_nperdip = 1 ;
  printf("setting nperdip to %d\n", sol_nperdip) ;
  complexvalflag = 0 ; 
  colscale = HEAT_SCALE ;
  overlayflag = TRUE ;
}

static void
sol_plot(int timept, int plot_type)
{
  int    ilat0,ilat1, nsum, ilat ;
  double avgval;
  int    vno, dip ;
  VERTEX *v ;
  double val ;

  if (!stc)
  {
    fprintf(stderr, "stc file not loaded.\n") ;
    return ;
  }

  surface_compiled = 0 ;
  if (sol_nperdip < 1)
  {
    fprintf(stderr, 
            "set # of dipoles per location (nperdip) before plotting\n") ;
    return ;
  }

  ilat0 = floor((timept-dlat/2-stc->epoch_begin_lat)/stc->sample_period+0.5);
  ilat1 = floor((timept+dlat/2-stc->epoch_begin_lat)/stc->sample_period+0.5);
  printf("ilat0=%d, ilat1=%d\n",ilat0,ilat1);
  nsum = ilat1-ilat0+1;

  MRISsetVals(mris, 0.0) ;
  MRISclearMarks(mris) ;
  for (dip = 0 ; dip < stc->nvertices ; dip++)

  {
    vno = stc->vertices[dip] ;
    if (vno < 0 || vno >= mris->nvertices)
    {
      printf("vertex # %d out of range\n", vno);
      return ;
    }
        
    v = &mris->vertices[vno] ; avgval = 0.0 ;
    for (ilat = ilat0 ; ilat <= ilat1 ; ilat++)
    {
      val = get_dipole_value(dip, ilat, plot_type, stc) ;
      avgval += val ;
    }
    avgval /= nsum ;

    /* put fMRI weighting in here potentially */

    v->val = avgval ;
    v->fixedval = 1 ;
  }
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->fixedval || v->ripflag)
      continue ;
    v->undefval = TRUE ;
  }
}

static double
get_dipole_value(int dipno, int ilat, int plot_type, STC *stc)
{
  double   sum, val ;
  int      i ;
  
  val = *MATRIX_RELT(stc->m_vals, dipno+1, ilat+1) ;
  switch (plot_type)
  {
  default:
  case 0:
    if (sol_nperdip == 1)
      val = fabs(val) ;
    else
    {
      /* take sqrt of sum of squares */
      sum = 0.0 ;
      for (i = 0 ; i < sol_nperdip ; i++)
      {
        val = *MATRIX_RELT(stc->m_vals, dipno+1+i, ilat+1) ;
        sum += (val*val) ;
      }
      val = sqrt(sum) ;
    }
    break ;
  case 1:
    if (sol_nperdip == 3)
    {
#if 0
      val = 
        *MATRIX_RELT(stc->m_vals, sol_nperdip*dip+0+1,ilat+1)*v->dipnx+
        *MATRIX_RELT(stc->m_vals, sol_nperdip*dip+1+1,ilat+1)*v->dipny+
        *MATRIX_RELT(stc->m_vals, sol_nperdip*dip+2+1,ilat+1)*v->dipnz;
#endif
    }
    break ;
  }
  return(val) ;
}
#if 1
double
dipval(int cond_num, int nzdip_num, int ilat)
{
#if 0
  int ic;
  double sum,val;

  val = 0 ;
  if (sol_plot_type==0)
  {
    sum = 0;
    for (ic=0;ic<sol_nperdip;ic++)
      sum += SQR(sol_dipcmp_val[cond_num][sol_nperdip*nzdip_num+ic][ilat]);
    val = sqrt(sum);
  } else
  if (sol_plot_type==1 || sol_plot_type==2)
  {
    if (sol_nperdip==1)
      val = sol_dipcmp_val[cond_num][sol_nperdip*nzdip_num][ilat];
    else
    {
      val = sol_dipcmp_val[cond_num][sol_nperdip*nzdip_num+0][ilat]*
              mris->vertices[sol_dipindex[nzdip_num]].dipnx +
            sol_dipcmp_val[cond_num][sol_nperdip*nzdip_num+1][ilat]*
              mris->vertices[sol_dipindex[nzdip_num]].dipny +
            sol_dipcmp_val[cond_num][sol_nperdip*nzdip_num+2][ilat]*
              mris->vertices[sol_dipindex[nzdip_num]].dipnz;
    }
    if (sol_plot_type==2) val = fabs(val);
  }
  return val;
#else
  return 0.0 ;
#endif
}
#endif
static void
remove_triangle_links(void)
{
  MRISremoveTriangleLinks(mris) ;
  surface_compiled = 0 ;
}

static void
drawcb(void)
{
  draw_colscalebar();
  glFlush() ;
}


static void
set_value_label_name(char *label_name, int field)
{
  char cmd[STRLEN] ;

  sprintf (cmd, "UpdateValueLabelName %d \"%s\"", field, label_name);
  Tcl_Eval (g_interp, cmd);
  sprintf (cmd, "ShowValueLabel %d 1", field);
  Tcl_Eval (g_interp, cmd);
  sprintf (cmd, "UpdateLinkedVarGroup view");
  Tcl_Eval (g_interp, cmd);
}
static int
draw_curvature_line(void)
{
  static int firsttime = 1 ;
  int    start_vno, current_vno, end_vno, n, best_n ;
  double dot, dx, dy, dz, odx, ody, odz, best_dot, len ;
  VERTEX *vn, *vend, *vstart, *vcurrent ;

  if (nmarked < 2)
  {
    fprintf(stderr, "must  origin and end points (previous two)\n");
    return(NO_ERROR) ;
  }

  if (firsttime)
  {
    MRISsaveVertexPositions(mris, TMP_VERTICES) ;
    read_inflated_vertex_coordinates() ;
    read_white_vertex_coordinates() ;
    MRISrestoreVertexPositions(mris, ORIGINAL_VERTICES) ;
    MRISaverageVertexPositions(mris, 25) ;
    MRIScomputeMetricProperties(mris) ;
    MRIScomputeSecondFundamentalForm(mris) ;  /* compute local basis in tangent bundle */
    MRISrestoreVertexPositions(mris, TMP_VERTICES) ;
    firsttime = 0 ;
  }

  start_vno = current_vno = marked[0] ; vstart = &mris->vertices[start_vno] ;
  end_vno = marked[1] ;  vend = &mris->vertices[end_vno] ;
  vend->marked = 0 ;

  odx = vend->x - vstart->x ; ody = vend->y - vstart->y ; odz = vend->z - vstart->z ;
  do
  {
    vcurrent = &mris->vertices[current_vno] ;
    vcurrent->marked = 1 ;
    odx = vend->infx - vcurrent->infx ; ody = vend->infy - vcurrent->infy ; odz = vend->infz - vcurrent->infz ;
    best_n = -1 ; best_dot = 0.0 ;
    for (n = 0 ; n < vcurrent->vnum ; n++)
    {
      vn = &mris->vertices[vcurrent->v[n]] ;
      if (vn->marked)
        continue ;   /* already in line */
      dx = vn->infx - vcurrent->infx ; dy = vn->infy - vcurrent->infy ; dz = vn->infz - vcurrent->infz ;
      dot = dx*odx + dy*ody + dz*odz ;
      if (dot < 0)
        continue ;
      dx = vn->x - vcurrent->x ; dy = vn->y - vcurrent->y ; dz = vn->z - vcurrent->z ;
      len = sqrt(dx*dx + dy*dy + dz*dz) ;
      if (FZERO(len))
        continue ;
      dx /= len ; dy /= len ; dz /= len ;
      dot = dx*vcurrent->e2x + dy*vcurrent->e2y + dz*vcurrent->e2z ;
      if (fabs(dot) > best_dot)
      {
        best_dot = fabs(dot) ;
        best_n = n ;
      }
    }
    if (best_n < 0)
      break ;
    draw_cursor(current_vno, FALSE) ;
    current_vno = vcurrent->v[best_n] ;
    if (current_vno != end_vno)
    {
#if 1
      marked[nmarked++] = current_vno ;
#else
      mark_vertex(current_vno, TRUE) ;
      draw_cursor(current_vno, TRUE) ;
    /*    redraw() ;*/
#endif
    }
  } while (current_vno != end_vno) ;

  redraw() ;
  return(NO_ERROR) ;
}

