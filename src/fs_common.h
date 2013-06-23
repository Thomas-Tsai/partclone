
struct fs_cmd_opt
{
    int debug;
    int ignore_fschk;
    int force;
};
typedef struct fs_cmd_opt fs_cmd_opt;

// This struct must be declared and initialised from main()
extern fs_cmd_opt fs_opt;
