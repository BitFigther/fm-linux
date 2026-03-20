/*
 * Copyright (c) 2025 BitFigther
 * Licensed under the MIT License
 */

#include <fnmatch.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <ftw.h>
#include <openssl/evp.h>
#include <openssl/md5.h>

int use_color = 1;
#define COLOR_RED    (use_color ? "\033[31m" : "")
#define COLOR_GREEN  (use_color ? "\033[32m" : "")
#define COLOR_YELLOW (use_color ? "\033[33m" : "")
#define COLOR_RESET  (use_color ? "\033[0m" : "")

#define BASELINE_FILE "/tmp/fm_baseline.dat"
#define MAX_BASELINE_FILES 8
char *baseline_file_paths[MAX_BASELINE_FILES];
int baseline_file_paths_count = 0;

// Structure to store baseline file information
typedef struct {
    char *filepath;
    time_t mtime;
    off_t size;
    unsigned char md5[MD5_DIGEST_LENGTH];
} FileInfo;

// Global variables
FileInfo *baseline = NULL;
int baseline_count = 0;
int baseline_capacity = 0;
int changes_detected = 0;
time_t baseline_time = 0;
char **exclude_patterns = NULL;
int exclude_patterns_count = 0;
int *file_checked = NULL;

void add_baseline_file_paths(const char *arg) {
    char *copy = strdup(arg);
    if (!copy) {
        fprintf(stderr, "Memory allocation error (strdup)\n");
        exit(1);
    }
    char *token = strtok(copy, ",");
    while (token) {
        if (baseline_file_paths_count < MAX_BASELINE_FILES) {
            char *p = strdup(token);
            if (!p) {
                fprintf(stderr, "Memory allocation error (strdup)\n");
                exit(1);
            }
            baseline_file_paths[baseline_file_paths_count++] = p;
        } else {
            fprintf(stderr, "Warning: Too many baseline files specified. Only the first %d will be used.\n", MAX_BASELINE_FILES);
            break;
        }
        token = strtok(NULL, ",");
    }
    free(copy);
}

void add_exclude_patterns(const char *arg) {
    char *copy = strdup(arg);
    if (!copy) {
        fprintf(stderr, "Memory allocation error (strdup)\n");
        exit(1);
    }
    char *token = strtok(copy, ",");
    while (token) {
        char **tmp = realloc(exclude_patterns, sizeof(char*) * (exclude_patterns_count + 1));
        if (!tmp) {
            fprintf(stderr, "Memory allocation error (realloc)\n");
            exit(1);
        }
        exclude_patterns = tmp;
        char *p = strdup(token);
        if (!p) {
            fprintf(stderr, "Memory allocation error (strdup)\n");
            exit(1);
        }
        exclude_patterns[exclude_patterns_count++] = p;
        token = strtok(NULL, ",");
    }
    free(copy);
}

/*
 * Returns 1 if fpath matches any user-specified --exclude pattern.
 * Each pattern is matched against the full path and against the basename,
 * using fnmatch() so that glob patterns like "*.tmp" or "/var/log/\*" work.
 * The match is attempted with FNM_PATHNAME so that "*" does not cross "/"
 * when the pattern contains a slash; without FNM_PATHNAME otherwise.
 */
static int is_user_excluded(const char *fpath) {
    const char *basename = strrchr(fpath, '/');
    basename = basename ? basename + 1 : fpath;

    for (int i = 0; i < exclude_patterns_count; i++) {
        const char *pat = exclude_patterns[i];
        int flags = strchr(pat, '/') ? FNM_PATHNAME : 0;
        if (fnmatch(pat, fpath, flags) == 0 ||
            fnmatch(pat, basename, 0) == 0) {
            return 1;
        }
    }
    return 0;
}

int calculate_md5(const char *filepath, unsigned char *result) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        return 0;
    }
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        fclose(file);
        return 0;
    }
    if (EVP_DigestInit_ex(ctx, EVP_md5(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        fclose(file);
        return 0;
    }
    unsigned char buffer[8192];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (EVP_DigestUpdate(ctx, buffer, bytes_read) != 1) {
            EVP_MD_CTX_free(ctx);
            fclose(file);
            return 0;
        }
    }
    unsigned int md_len;
    if (EVP_DigestFinal_ex(ctx, result, &md_len) != 1) {
        EVP_MD_CTX_free(ctx);
        fclose(file);
        return 0;
    }
    EVP_MD_CTX_free(ctx);
    fclose(file);
    return 1;
}

void md5_to_string(const unsigned char *md5, char *output) {
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", md5[i]);
    }
    output[MD5_DIGEST_LENGTH * 2] = '\0';
}

void add_file_info(const char *filepath, time_t mtime, off_t size, const unsigned char *md5) {
    if (baseline_count >= baseline_capacity) {
        baseline_capacity = baseline_capacity == 0 ? 1000 : baseline_capacity * 2;
        baseline = realloc(baseline, baseline_capacity * sizeof(FileInfo));
        if (!baseline) {
            fprintf(stderr, "Memory allocation error\n");
            exit(1);
        }
    }
    baseline[baseline_count].filepath = strdup(filepath);
    baseline[baseline_count].mtime = mtime;
    baseline[baseline_count].size = size;
    memcpy(baseline[baseline_count].md5, md5, MD5_DIGEST_LENGTH);
    baseline_count++;
}

FileInfo* find_file_info(const char *filepath) {
    for (int i = 0; i < baseline_count; i++) {
        if (strcmp(baseline[i].filepath, filepath) == 0) {
            return &baseline[i];
        }
    }
    return NULL;
}

int scan_file(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag != FTW_F) {
        return 0;
    }
    if (is_user_excluded(fpath)) {
        return 0;
    }
    /* Auto-exclude system paths that are unsafe or irrelevant to scan */
    if (strstr(fpath, "/tmp/") ||
        strstr(fpath, "/var/log/") ||
        strstr(fpath, "/proc/") ||
        strstr(fpath, "/sys/") ||
        strstr(fpath, "/dev/")) {
        return 0;
    }
    unsigned char md5[MD5_DIGEST_LENGTH];
    if (!calculate_md5(fpath, md5)) {
        return 0;
    }
    if (baseline_time == 0) {
        add_file_info(fpath, sb->st_mtime, sb->st_size, md5);
        return 0;
    }
    FileInfo *existing = find_file_info(fpath);
    if (existing) {
        for (int i = 0; i < baseline_count; i++) {
            if (strcmp(baseline[i].filepath, fpath) == 0) {
                file_checked[i] = 1;
                break;
            }
        }
        
        int hash_changed = memcmp(existing->md5, md5, MD5_DIGEST_LENGTH) != 0;
        int mtime_changed = existing->mtime != sb->st_mtime;
        int size_changed = existing->size != sb->st_size;
            if (hash_changed || mtime_changed || size_changed) {
                printf("%sChange detected: %s%s\n", COLOR_YELLOW, fpath, COLOR_RESET);
                if (mtime_changed) {
                    char old_time_str[32], new_time_str[32];
                    struct tm tm_old, tm_new;
                    localtime_r(&existing->mtime, &tm_old);
                    localtime_r(&sb->st_mtime, &tm_new);
                    strftime(old_time_str, sizeof(old_time_str), "%Y%m%d_%H%M%S", &tm_old);
                    strftime(new_time_str, sizeof(new_time_str), "%Y%m%d_%H%M%S", &tm_new);
                    printf("  Modified time: %s -> %s\n", old_time_str, new_time_str);
                }
                if (size_changed) {
                    printf("  Size: %ld -> %ld\n", existing->size, sb->st_size);
                }
                if (hash_changed) {
                    char old_hash[MD5_DIGEST_LENGTH * 2 + 1];
                    char new_hash[MD5_DIGEST_LENGTH * 2 + 1];
                    md5_to_string(existing->md5, old_hash);
                    md5_to_string(md5, new_hash);
                    printf("  MD5 hash: %s -> %s\n", old_hash, new_hash);
                }
                changes_detected++;
            }
    } else {
        char hash_str[MD5_DIGEST_LENGTH * 2 + 1];
        md5_to_string(md5, hash_str);
    printf("%sNew file: %s (MD5: %s)%s\n", COLOR_GREEN, fpath, hash_str, COLOR_RESET);
        changes_detected++;
    }
    return 0;
}

void save_baseline() {
    for (int fidx = 0; fidx < baseline_file_paths_count; fidx++) {
        FILE *fp = fopen(baseline_file_paths[fidx], "wb");
        if (!fp) {
            fprintf(stderr, "Failed to create baseline file: %s\n", baseline_file_paths[fidx]);
            continue;
        }
        time_t current_time = time(NULL);
        fwrite(&current_time, sizeof(time_t), 1, fp);
        fwrite(&baseline_count, sizeof(int), 1, fp);
        for (int i = 0; i < baseline_count; i++) {
            int path_len = strlen(baseline[i].filepath) + 1;
            fwrite(&path_len, sizeof(int), 1, fp);
            fwrite(baseline[i].filepath, path_len, 1, fp);
            fwrite(&baseline[i].mtime, sizeof(time_t), 1, fp);
            fwrite(&baseline[i].size, sizeof(off_t), 1, fp);
            fwrite(baseline[i].md5, MD5_DIGEST_LENGTH, 1, fp);
        }
        fclose(fp);
        printf("Create baseline file : %s \n", baseline_file_paths[fidx]);
        printf("Baseline saved: %d files\n", baseline_count);
    }
}

int load_baseline() {
    int loaded = 0;
    for (int fidx = 0; fidx < baseline_file_paths_count; fidx++) {
        FILE *fp = fopen(baseline_file_paths[fidx], "rb");
        if (!fp) {
            continue;
        }
        if (fread(&baseline_time, sizeof(time_t), 1, fp) != 1) {
            fclose(fp);
            break;
        }
        if (fread(&baseline_count, sizeof(int), 1, fp) != 1) {
            fclose(fp);
            break;
        }
        baseline_capacity = baseline_count;
        baseline = malloc(baseline_capacity * sizeof(FileInfo));
        if (!baseline) {
            fclose(fp);
            break;
        }
        int failed = 0;
        for (int i = 0; i < baseline_count; i++) {
            int path_len;
            if (fread(&path_len, sizeof(int), 1, fp) != 1) { failed = 1; break; }
            baseline[i].filepath = malloc(path_len);
            if (!baseline[i].filepath || fread(baseline[i].filepath, path_len, 1, fp) != 1) { failed = 1; break; }
            if (fread(&baseline[i].mtime, sizeof(time_t), 1, fp) != 1 ||
                fread(&baseline[i].size, sizeof(off_t), 1, fp) != 1 ||
                fread(baseline[i].md5, MD5_DIGEST_LENGTH, 1, fp) != 1) { failed = 1; break; }
        }
        fclose(fp);
        if (failed) {
            for (int i = 0; i < baseline_count; i++) {
                free(baseline[i].filepath);
            }
            free(baseline);
            baseline = NULL;
            break;
        }
        char time_str[32];
        struct tm tm_baseline;
        localtime_r(&baseline_time, &tm_baseline);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_baseline);
        printf("Baseline loaded: %d files (Created: %s)\n", baseline_count, time_str);
        if (file_checked) { 
            free(file_checked); file_checked = NULL; 
        }
        file_checked = calloc(baseline_count, sizeof(int));
        if (!file_checked) {
            for (int i = 0; i < baseline_count; i++) free(baseline[i].filepath);
            free(baseline);
            baseline = NULL;
            break;
        }
        loaded = 1;
        break;
    }
    return loaded;
}

void report_deleted_files() {
    if (!file_checked) return;
    for (int i = 0; i < baseline_count; i++) {
        if (is_user_excluded(baseline[i].filepath)) continue;
        if (!file_checked[i]) {
            printf("%sDeleted file: %s%s\n", COLOR_RED, baseline[i].filepath, COLOR_RESET);
            changes_detected++;
        }
    }
}

void add_target_dirs(const char *arg, char ***target_dirs, int *target_dirs_count) {
    char *copy = strdup(arg);
    if (!copy) {
        fprintf(stderr, "Memory allocation error (strdup)\n");
        exit(1);
    }
    char *token = strtok(copy, ",");
    while (token) {
    char **tmp = realloc(*target_dirs, sizeof(char*) * (*target_dirs_count + 1));
        if (!tmp) {
            fprintf(stderr, "Memory allocation error (realloc)\n");
            exit(1);
        }
    *target_dirs = tmp;
        char *p = strdup(token);
        if (!p) {
            fprintf(stderr, "Memory allocation error (strdup)\n");
            exit(1);
        }
        (*target_dirs)[(*target_dirs_count)++] = p;
        token = strtok(NULL, ",");
    }
    free(copy);
}

void print_usage(const char *program_name) {
    printf("Usage:\n");
    printf("  %s --baseline [directory...] [options] : Create baseline (with MD5 hash)\n", program_name);
    printf("  %s --check [directory...]    [options] : Check for changes (strict MD5 check)\n", program_name);
    printf("  %s --reset [options]                   : Reset baseline\n", program_name);
    printf("\n");
    printf("Required options (choose exactly one):\n");
    printf("  --baseline, -B    Create baseline\n");
    printf("  --check,    -C    Check for changes\n");
    printf("  --reset,    -R    Reset (delete) baseline file\n");
    printf("\n");
    printf("Optional options:\n");
    printf("  --exclude, -e <path(,path...)>           Exclude path(s) from scan\n");
    printf("  --baseline-file, -b <path(,path...)>     Specify baseline file path(s)\n");
    printf("  --no-color                               Disable colored output\n");
    printf("\n");
    printf("Note: Options and directories can appear in any order.\n");
    printf("      --exclude/-e may be specified multiple times.\n");
}

int main(int argc, char *argv[]) {
    char **target_dirs = NULL;
    int target_dirs_count = 0;
    int baseline_file_explicit = 0;
    int mode = 0; /* 'B'=baseline, 'C'=check, 'R'=reset */

    baseline_file_paths_count = 0;

    static struct option long_options[] = {
        {"baseline",      no_argument,       NULL, 'B'},
        {"check",         no_argument,       NULL, 'C'},
        {"reset",         no_argument,       NULL, 'R'},
        {"exclude",       required_argument, NULL, 'e'},
        {"baseline-file", required_argument, NULL, 'b'},
        {"no-color",      no_argument,       NULL, 'N'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "BCRe:b:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'B':
            case 'C':
            case 'R':
                if (mode != 0) {
                    fprintf(stderr, "Error: --baseline, --check, and --reset are mutually exclusive.\n");
                    return 1;
                }
                mode = opt;
                break;
            case 'e':
                add_exclude_patterns(optarg);
                break;
            case 'b':
                baseline_file_paths_count = 0;
                add_baseline_file_paths(optarg);
                baseline_file_explicit = 1;
                break;
            case 'N':
                use_color = 0;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    /* Remaining non-option arguments are target directories */
    for (int i = optind; i < argc; i++) {
        add_target_dirs(argv[i], &target_dirs, &target_dirs_count);
    }

    if (!baseline_file_explicit) {
        baseline_file_paths[baseline_file_paths_count++] = strdup(BASELINE_FILE);
    }

    if (mode == 0) {
        print_usage(argv[0]);
        return 1;
    }

    if (mode == 'R') {
        int ret = 0;
        for (int fidx = 0; fidx < baseline_file_paths_count; fidx++) {
            if (unlink(baseline_file_paths[fidx]) == 0) {
                printf("Baseline file deleted: %s\n", baseline_file_paths[fidx]);
            } else {
                fprintf(stderr, "Baseline file not found: %s\n", baseline_file_paths[fidx]);
                ret = 1;
            }
        }
        return ret;
    }

    if (target_dirs_count == 0) {
        fprintf(stderr, "Error: No target directory specified.\n");
        print_usage(argv[0]);
        if (exclude_patterns) {
            for (int i = 0; i < exclude_patterns_count; i++) free(exclude_patterns[i]);
            free(exclude_patterns);
        }
        return 1;
    }

    int ret = 0;
    if (mode == 'B') {
        printf("Creating baseline for:");
        for (int i = 0; i < target_dirs_count; i++) {
            printf(" %s", target_dirs[i]);
        }
        printf("\nProcessing...\n");
        int err = 0;
        for (int i = 0; i < target_dirs_count; i++) {
            if (nftw(target_dirs[i], scan_file, 20, FTW_PHYS) == -1) {
                perror("Directory scan error");
                err = 1;
            }
        }
        if (!err) save_baseline();
        ret = err;
    } else { /* mode == 'C' */
        printf("Checking for changes in:");
        for (int i = 0; i < target_dirs_count; i++) {
            printf(" %s", target_dirs[i]);
        }
        printf("\n");
        if (!load_baseline()) {
            printf("Error: Baseline file not found.\n");
            printf("Please create a baseline first using --baseline or -B option.\n");
            ret = 1;
        } else {
            printf("Processing...\n");
            changes_detected = 0;
            int err = 0;
            for (int i = 0; i < target_dirs_count; i++) {
                if (nftw(target_dirs[i], scan_file, 20, FTW_PHYS) == -1) {
                    perror("Directory scan error");
                    err = 1;
                }
            }
            if (!err) {
                report_deleted_files();
                printf("\n=== Result ===\n");
                if (changes_detected > 0) {
                    printf("Changes detected: %d file(s) changed\n", changes_detected);
                    ret = 2;
                } else {
                    printf("No changes: No files were changed\n");
                    ret = 0;
                }
            } else {
                ret = 1;
            }
        }
    }

    for (int i = 0; i < baseline_count; i++) free(baseline[i].filepath);
    free(baseline);
    for (int i = 0; i < exclude_patterns_count; i++) free(exclude_patterns[i]);
    free(exclude_patterns);
    if (target_dirs) {
        for (int i = 0; i < target_dirs_count; i++) free(target_dirs[i]);
        free(target_dirs);
    }
    if (file_checked) { free(file_checked); file_checked = NULL; }
    for (int i = 0; i < baseline_file_paths_count; i++) free(baseline_file_paths[i]);
    return ret;
}
