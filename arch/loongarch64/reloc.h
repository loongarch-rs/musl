#ifdef __loongarch_soft_float
#define FP_SUFFIX "-sf"
#else
#define FP_SUFFIX ""
#endif

#define LDSO_ARCH "loongarch64"  FP_SUFFIX

#define TPOFF_K         0

#define REL_PLT         R_LARCH_JUMP_SLOT
#define REL_COPY        R_LARCH_COPY
#define REL_DTPMOD      R_LARCH_TLS_DTPMOD64
#define REL_DTPOFF      R_LARCH_TLS_DTPREL64
#define REL_TPOFF       R_LARCH_TLS_TPREL64
#define REL_RELATIVE    R_LARCH_RELATIVE
#define REL_SYMBOLIC    R_LARCH_64

#define CRTJMP(pc,sp) __asm__ __volatile__( \
	"move $sp, %1 ; jr %0" : : "r"(pc), "r"(sp) : "memory" )
