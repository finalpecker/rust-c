#include "compiler.h"
#include "pipeline.h"

int mini_rustc_run_file(const char *path, long long *out_result) {
    return compiler_pipeline_run_file(path, out_result);
}
