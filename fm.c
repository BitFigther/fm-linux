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

// プロトタイプ宣言
void add_target_dirs(const char *arg, const char ***target_dirs, int *target_dirs_count);

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
// Exclude patterns (from args)
char **exclude_patterns = NULL;
int exclude_patterns_count = 0;

// カンマ区切り文字列を分割してexclude_patternsに追加
void add_exclude_patterns(const char *arg) {
    char *copy = strdup(arg);
    char *token = strtok(copy, ",");
    while (token) {
        exclude_patterns = realloc(exclude_patterns, sizeof(char*) * (exclude_patterns_count + 1));
        exclude_patterns[exclude_patterns_count++] = strdup(token);
        token = strtok(NULL, ",");
    }
    free(copy);
}

// Path to baseline file (with timestamp)
char baseline_file_path_buf[256] = "";
char *baseline_file_path = baseline_file_path_buf;

void set_default_baseline_file_path() {
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    snprintf(baseline_file_path_buf, sizeof(baseline_file_path_buf),
             "/tmp/fm_baseline_%04d%02d%02d_%02d%02d%02d.dat",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
}

// Function to calculate MD5 hash of a file (OpenSSL 3.0 compatible)
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

// Convert MD5 hash to hex string
void md5_to_string(const unsigned char *md5, char *output) {
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", md5[i]);
    }
    output[MD5_DIGEST_LENGTH * 2] = '\0';
}

// Function to add file info to baseline
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

// Function to find file info in baseline
FileInfo* find_file_info(const char *filepath) {
    for (int i = 0; i < baseline_count; i++) {
        if (strcmp(baseline[i].filepath, filepath) == 0) {
            return &baseline[i];
        }
    }
    return NULL;
}

// Callback function called when scanning files
int scan_file(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    // Ignore directories and symbolic links
    if (typeflag != FTW_F) {
        return 0;
    }
    // Exclude by user-specified patterns
    for (int i = 0; i < exclude_patterns_count; i++) {
        if (strstr(fpath, exclude_patterns[i])) {
            return 0;
        }
    }
    // 自動除外ディレクトリ
    if (strstr(fpath, "/tmp/") ||
        strstr(fpath, "/var/log/") ||
        strstr(fpath, "/proc/") ||
        strstr(fpath, "/sys/") ||
        strstr(fpath, "/dev/")) {
        return 0;
    }
    // Calculate MD5 hash
    unsigned char md5[MD5_DIGEST_LENGTH];
    if (!calculate_md5(fpath, md5)) {
        // File read error (e.g. permission denied)
        return 0;
    }
    // Baseline creation mode
    if (baseline_time == 0) {
        add_file_info(fpath, sb->st_mtime, sb->st_size, md5);
        return 0;
    }
    // Change detection mode
    FileInfo *existing = find_file_info(fpath);
    if (existing) {
        // Check for changes in existing file
        int hash_changed = memcmp(existing->md5, md5, MD5_DIGEST_LENGTH) != 0;
        int mtime_changed = existing->mtime != sb->st_mtime;
        int size_changed = existing->size != sb->st_size;
        if (hash_changed || mtime_changed || size_changed) {
            printf("Change detected: %s\n", fpath);
            if (mtime_changed) {
                printf("  Modified time: %ld -> %ld\n", existing->mtime, sb->st_mtime);
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
        // New file
        char hash_str[MD5_DIGEST_LENGTH * 2 + 1];
        md5_to_string(md5, hash_str);
        printf("New file: %s (MD5: %s)\n", fpath, hash_str);
        changes_detected++;
    }
    return 0;
}

// Save baseline to file
void save_baseline() {
    FILE *fp = fopen(baseline_file_path, "wb");
    if (!fp) {
        perror("Failed to create baseline file");
        return;
    }
    // Save baseline creation time
    time_t current_time = time(NULL);
    fwrite(&current_time, sizeof(time_t), 1, fp);
    // Save file count
    fwrite(&baseline_count, sizeof(int), 1, fp);
    // Save each file info
    for (int i = 0; i < baseline_count; i++) {
        int path_len = strlen(baseline[i].filepath) + 1;
        fwrite(&path_len, sizeof(int), 1, fp);
        fwrite(baseline[i].filepath, path_len, 1, fp);
        fwrite(&baseline[i].mtime, sizeof(time_t), 1, fp);
        fwrite(&baseline[i].size, sizeof(off_t), 1, fp);
        fwrite(baseline[i].md5, MD5_DIGEST_LENGTH, 1, fp);
    }
    fclose(fp);
    printf("Baseline saved: %d files\n", baseline_count);
}

// Load baseline from file
int load_baseline() {
    FILE *fp = fopen(baseline_file_path, "rb");
    if (!fp) {
        return 0; // Baseline file does not exist
    }
    // Read baseline creation time
    if (fread(&baseline_time, sizeof(time_t), 1, fp) != 1) {
        fclose(fp);
        return 0;
    }
    // Read file count
    if (fread(&baseline_count, sizeof(int), 1, fp) != 1) {
        fclose(fp);
        return 0;
    }
    // Allocate memory
    baseline_capacity = baseline_count;
    baseline = malloc(baseline_capacity * sizeof(FileInfo));
    if (!baseline) {
        fclose(fp);
        return 0;
    }
    // Read each file info
    for (int i = 0; i < baseline_count; i++) {
        int path_len;
        if (fread(&path_len, sizeof(int), 1, fp) != 1) {
            fclose(fp);
            return 0;
        }
        baseline[i].filepath = malloc(path_len);
        if (fread(baseline[i].filepath, path_len, 1, fp) != 1) {
            fclose(fp);
            return 0;
        }
        if (fread(&baseline[i].mtime, sizeof(time_t), 1, fp) != 1 ||
            fread(&baseline[i].size, sizeof(off_t), 1, fp) != 1 ||
            fread(baseline[i].md5, MD5_DIGEST_LENGTH, 1, fp) != 1) {
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    printf("Baseline loaded: %d files (Created: %s)", baseline_count, ctime(&baseline_time));
    return 1;
}


// カンマ区切り文字列を分割してtarget_dirsに追加
void add_target_dirs(const char *arg, const char ***target_dirs, int *target_dirs_count) {
    char *copy = strdup(arg);
    char *token = strtok(copy, ",");
    while (token) {
        *target_dirs = realloc((void*)*target_dirs, sizeof(char*) * (*target_dirs_count + 1));
        (*target_dirs)[(*target_dirs_count)++] = strdup(token);
        token = strtok(NULL, ",");
    }
    free(copy);
}

// Print usage
void print_usage(const char *program_name) {
    printf("Usage:\n");
    printf("  %s --baseline|-B [directory(,directory...)] [--exclude|-e path(,path...)] [--baseline-file|-b path] : Create baseline (with MD5 hash)\n", program_name);
    printf("  %s --check|-c [directory(,directory...)] [--exclude|-e path(,path...)] [--baseline-file|-b path]    : Check for changes (strict MD5 check)\n", program_name);
    printf("  %s --reset|-r [--baseline-file|-b path]                                                    : Reset baseline\n", program_name);
    printf("\n");
    printf("Examples:\n");
    printf("  %s --baseline /,/usr --exclude /tmp/,/var/log/ --baseline-file /tmp/mybase.dat     : Create baseline for / and /usr, excluding /tmp/, /var/log/, baseline file is /tmp/mybase.dat\n", program_name);
    printf("  %s -B /,/usr -e /tmp/,/var/log/ -b /tmp/mybase.dat                                 : (same as above, short options)\n", program_name);
    printf("  %s --check /etc,/opt --exclude /proc/ --baseline-file /tmp/mybase.dat              : Check for changes in /etc and /opt, excluding /proc/, using /tmp/mybase.dat\n", program_name);
    printf("  %s -c /etc,/opt -e /proc/ -b /tmp/mybase.dat                                      : (same as above, short options)\n", program_name);
    printf("  %s --baseline /usr                                : Create baseline for /usr\n", program_name);
    printf("\n");
    printf("Note: MD5 hash calculation may take time, but enables strict change detection.\n");
    printf("      You can specify multiple directories and --exclude/-e multiple times, each with a comma-separated list.\n");
}

int main(int argc, char *argv[]) {
    set_default_baseline_file_path();
    const char **target_dirs = NULL;
    int target_dirs_count = 0;
    int reset_requested = 0;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    // 最初にすべてのオプションを解析
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--exclude") == 0 || strcmp(argv[i], "-e") == 0) && i + 1 < argc) {
            add_exclude_patterns(argv[++i]);
        } else if ((strcmp(argv[i], "--baseline-file") == 0 || strcmp(argv[i], "-b") == 0) && i + 1 < argc) {
            baseline_file_path = argv[++i];
        } else if (strcmp(argv[i], "--reset") == 0 || strcmp(argv[i], "-r") == 0) {
            reset_requested = 1;
        } else if (strcmp(argv[i], "--baseline") == 0 || strcmp(argv[i], "-B") == 0 || strcmp(argv[i], "--check") == 0 || strcmp(argv[i], "-c") == 0) {
            // skip, handled below
        } else if (argv[i][0] != '-') {
            add_target_dirs(argv[i], &target_dirs, &target_dirs_count);
        }
    }

    // リセット要求がある場合は処理
    if (reset_requested) {
        if (unlink(baseline_file_path) == 0) {
            printf("Baseline file deleted: %s\n", baseline_file_path);
        } else {
            printf("Baseline file not found: %s\n", baseline_file_path);
        }
        // リセットのみの場合は終了
        if (!(strcmp(argv[1], "--baseline") == 0 || strcmp(argv[1], "-B") == 0 || strcmp(argv[1], "--check") == 0 || strcmp(argv[1], "-c") == 0)) {
            return 0;
        }
    }
    
    if (target_dirs_count == 0 || 
        !(strcmp(argv[1], "--baseline") == 0 || strcmp(argv[1], "-B") == 0 || strcmp(argv[1], "--check") == 0 || strcmp(argv[1], "-c") == 0)) {
        print_usage(argv[0]);
        if (exclude_patterns) {
            for (int i = 0; i < exclude_patterns_count; i++) free(exclude_patterns[i]);
            free(exclude_patterns);
        }
        if (target_dirs) free(target_dirs);
        return 1;
    }

    int ret = 0;
    if (strcmp(argv[1], "--baseline") == 0 || strcmp(argv[1], "-B") == 0) {
        printf("Creating baseline for:");
        for (int i = 0; i < target_dirs_count; i++) printf(" %s", target_dirs[i]);
        printf("\nProcessing...\n");
        // Scan file tree and create baseline for all dirs
        int err = 0;
        for (int i = 0; i < target_dirs_count; i++) {
            if (nftw(target_dirs[i], scan_file, 20, FTW_PHYS) == -1) {
                perror("Directory scan error");
                err = 1;
            }
        }
        if (!err) save_baseline();
        ret = err;
    } else if (strcmp(argv[1], "--check") == 0 || strcmp(argv[1], "-c") == 0) {
        printf("Checking for changes in:");
        for (int i = 0; i < target_dirs_count; i++) printf(" %s", target_dirs[i]);
        printf("\n");
        if (!load_baseline()) {
            printf("Error: Baseline file not found.\n");
            printf("Please create a baseline first using --baseline option.\n");
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
                printf("\n=== Result ===\n");
                if (changes_detected > 0) {
                    printf("Changes detected: %d file(s) changed\n", changes_detected);
                    ret = 2; // Exit code for changes detected
                } else {
                    printf("No changes: No files were changed\n");
                    ret = 0; // Exit code for no changes
                }
            } else {
                ret = 1;
            }
        }
    } else {
        print_usage(argv[0]);
        ret = 1;
    }
    // Free memory
    for (int i = 0; i < baseline_count; i++) {
        free(baseline[i].filepath);
    }
    free(baseline);
    for (int i = 0; i < exclude_patterns_count; i++) {
        free(exclude_patterns[i]);
    }
    free(exclude_patterns);
    if (target_dirs) {
        for (int i = 0; i < target_dirs_count; i++) free((void*)target_dirs[i]);
        free(target_dirs);
    }
    return ret;
}
