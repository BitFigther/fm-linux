#include <fnmatch.h>
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

// 色付け有無のグローバル変数
int use_color = 1;
#define COLOR_RED    (use_color ? "\033[31m" : "")
#define COLOR_GREEN  (use_color ? "\033[32m" : "")
#define COLOR_YELLOW (use_color ? "\033[33m" : "")
#define COLOR_RESET  (use_color ? "\033[0m" : "")

// デフォルトのベースラインファイルパスをマクロで定義
#define BASELINE_FILE "/tmp/fm_baseline.dat"

// Path(s) to baseline file(s)
#define MAX_BASELINE_FILES 16
char *baseline_file_paths[MAX_BASELINE_FILES];
int baseline_file_paths_count = 0;

// 複数パスをカンマ区切りで追加
void add_baseline_file_paths(const char *arg) {
    char *copy = strdup(arg);
    char *token = strtok(copy, ",");
    while (token && baseline_file_paths_count < MAX_BASELINE_FILES) {
        baseline_file_paths[baseline_file_paths_count++] = strdup(token);
        token = strtok(NULL, ",");
    }
    free(copy);
}

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
// ファイルの存在チェック用配列を追加
int *file_checked = NULL;

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
        // このファイルが存在することをマーク
        for (int i = 0; i < baseline_count; i++) {
            if (strcmp(baseline[i].filepath, fpath) == 0) {
                file_checked[i] = 1;
                break;
            }
        }
        
        // Check for changes in existing file
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
        // New file
        char hash_str[MD5_DIGEST_LENGTH * 2 + 1];
        md5_to_string(md5, hash_str);
    printf("%sNew file: %s (MD5: %s)%s\n", COLOR_GREEN, fpath, hash_str, COLOR_RESET);
        changes_detected++;
    }
    return 0;
}

// Save baseline to file
void save_baseline() {
    for (int fidx = 0; fidx < baseline_file_paths_count; fidx++) {
        FILE *fp = fopen(baseline_file_paths[fidx], "wb");
        if (!fp) {
            fprintf(stderr, "Failed to create baseline file: %s\n", baseline_file_paths[fidx]);
            continue;
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
        printf("Create baseline file : %s \n", baseline_file_paths[fidx]);
        printf("Baseline saved: %d files\n", baseline_count);
    }
}

// Load baseline from file
int load_baseline() {
    int loaded = 0;
    for (int fidx = 0; fidx < baseline_file_paths_count; fidx++) {
        FILE *fp = fopen(baseline_file_paths[fidx], "rb");
        if (!fp) {
            continue; // Baseline file does not exist
        }
        // Read baseline creation time
        if (fread(&baseline_time, sizeof(time_t), 1, fp) != 1) {
            fclose(fp);
            continue;
        }
        // Read file count
        if (fread(&baseline_count, sizeof(int), 1, fp) != 1) {
            fclose(fp);
            continue;
        }
        // Allocate memory
        baseline_capacity = baseline_count;
        baseline = malloc(baseline_capacity * sizeof(FileInfo));
        if (!baseline) {
            fclose(fp);
            continue;
        }
        // Read each file info
        for (int i = 0; i < baseline_count; i++) {
            int path_len;
            if (fread(&path_len, sizeof(int), 1, fp) != 1) {
                fclose(fp);
                continue;
            }
            baseline[i].filepath = malloc(path_len);
            if (fread(baseline[i].filepath, path_len, 1, fp) != 1) {
                fclose(fp);
                continue;
            }
            if (fread(&baseline[i].mtime, sizeof(time_t), 1, fp) != 1 ||
                fread(&baseline[i].size, sizeof(off_t), 1, fp) != 1 ||
                fread(baseline[i].md5, MD5_DIGEST_LENGTH, 1, fp) != 1) {
                fclose(fp);
                continue;
            }
        }
        fclose(fp);
        printf("Baseline loaded: %d files (Created: %s)", baseline_count, ctime(&baseline_time));
        // ファイルチェック配列を初期化（チェックモード用）
        file_checked = calloc(baseline_count, sizeof(int));
        if (!file_checked) {
            continue;
        }
        loaded = 1;
        break; // 最初に見つかったファイルのみロード
    }
    return loaded;
}

// 削除されたファイルを報告する関数
void report_deleted_files() {
    if (!file_checked) return;
    
    for (int i = 0; i < baseline_count; i++) {
        // 除外パターンに該当する場合はスキップ
        int excluded = 0;
        for (int j = 0; j < exclude_patterns_count; j++) {
            if (strstr(baseline[i].filepath, exclude_patterns[j])) {
                excluded = 1;
                break;
            }
        }
        if (excluded) continue;
        if (!file_checked[i]) {
            printf("%sDeleted file: %s%s\n", COLOR_RED, baseline[i].filepath, COLOR_RESET);
            changes_detected++;
        }
    }
    
    free(file_checked);
    file_checked = NULL;
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
    printf("  %s --baseline|-B [directory(,directory...)] [options] : Create baseline (with MD5 hash)\n", program_name);
    printf("  %s --check|-C [directory(,directory...)] [options]    : Check for changes (strict MD5 check)\n", program_name);
    printf("  %s --reset|-R [options]                               : Reset baseline\n", program_name);
    printf("\n");
    printf("Required options (choose one):\n");
    printf("  --baseline, -B    Create baseline\n");
    printf("  --check, -C       Check for changes\n");
    printf("  --reset, -R       Reset (delete) baseline file\n");
    printf("\n");
    printf("Optional options:\n");
    printf("  --exclude, -e <path(,path...)>    Exclude path(s) from scan\n");
    printf("  --baseline-file, -b <path(,path...)> Specify one or more baseline file paths (comma-separated)\n");
    printf("  --no-color                        Disable colored output\n");
    printf("\n");
    printf("Note: MD5 hash calculation may take time, but enables strict change detection.\n");
    printf("      You can specify multiple directories and --exclude/-e multiple times, each with a comma-separated list.\n");
}

int main(int argc, char *argv[]) {
    const char **target_dirs = NULL;
    int target_dirs_count = 0;
    int reset_requested = 0;

    // baseline_file_paths初期値
    baseline_file_paths_count = 0;
    baseline_file_paths[baseline_file_paths_count++] = strdup(BASELINE_FILE);

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    // 最初にすべてのオプションを解析
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-color") == 0) {
            use_color = 0;
        } else if ((strcmp(argv[i], "--exclude") == 0 || strcmp(argv[i], "-e") == 0) && i + 1 < argc) {
            add_exclude_patterns(argv[++i]);
        } else if ((strcmp(argv[i], "--baseline-file") == 0 || strcmp(argv[i], "-b") == 0) && i + 1 < argc) {
            baseline_file_paths_count = 0; // 明示的指定があれば初期値をリセット
            add_baseline_file_paths(argv[++i]);
        } else if (strcmp(argv[i], "--reset") == 0 || strcmp(argv[i], "-R") == 0) {
            reset_requested = 1;
        } else if (strcmp(argv[i], "--baseline") == 0 || strcmp(argv[i], "-B") == 0 || strcmp(argv[i], "--check") == 0 || strcmp(argv[i], "-C") == 0) {
            // skip, handled below
        } else if (argv[i][0] != '-') {
            add_target_dirs(argv[i], &target_dirs, &target_dirs_count);
        }
    }

    // リセット要求がある場合は処理
    if (reset_requested) {
        int reset_failed = 0;  // リセット失敗フラグを追加
        for (int fidx = 0; fidx < baseline_file_paths_count; fidx++) {
            if (unlink(baseline_file_paths[fidx]) == 0) {
                printf("Baseline file deleted: %s\n", baseline_file_paths[fidx]);
            } else {
                printf("Baseline file not found: %s\n", baseline_file_paths[fidx]);
                reset_failed = 1;  // 削除に失敗した場合はフラグを設定
            }
        }
        // リセットのみの場合は終了
        if (!(strcmp(argv[1], "--baseline") == 0 || strcmp(argv[1], "-B") == 0 || 
              strcmp(argv[1], "--check") == 0 || strcmp(argv[1], "-C") == 0)) {
            return reset_failed;  // リセット失敗時は1、成功時は0を返す
        }
    }
    
    if (target_dirs_count == 0 || 
        !(strcmp(argv[1], "--baseline") == 0 || strcmp(argv[1], "-B") == 0 || strcmp(argv[1], "--check") == 0 || strcmp(argv[1], "-C") == 0)) {
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
    } else if (strcmp(argv[1], "--check") == 0 || strcmp(argv[1], "-C") == 0) {
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
            
            // 削除されたファイルのレポートを追加
            if (!err) {
                report_deleted_files();
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
    // file_checked の解放（万一解放されていない場合のため）
    free(file_checked);
    // baseline_file_paths の解放
    for (int i = 0; i < baseline_file_paths_count; i++) {
        free(baseline_file_paths[i]);
    }
    return ret;
}
