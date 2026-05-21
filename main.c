#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_FILES 32
#define MAX_TOTAL_SIZE (200 * 1024 * 1024) // 200 MB sınırı

/* * Bir dosyanın sadece ASCII (metin) karakterlerinden oluşup oluşmadığını kontrol eder.
 * Karakter başına 1 bayt olmalı ve 0-127 aralığında olmalıdır.
 */
int is_ascii_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return 0;

    int ch;
    while ((ch = fgetc(file)) != EOF) {
        if (ch < 0 || ch > 127) {
            fclose(file);
            return 0; // ASCII olmayan karakter bulundu
        }
    }
    fclose(file);
    return 1; // Dosya tamamen ASCII metindir
}

/* * -b Parametresi: Giriş dosyalarını kontrol eder, doğrular ve tek bir .sau arşivinde birleştirir.
 */
void archive_files(int argc, char *argv[]) {
    char *output_filename = "a.sau"; // Varsayılan arşiv adı
    int file_count = 0;
    char *input_files[MAX_FILES];

    // Komut satırı argümanlarını oku, giriş dosyalarını ve varsa -o parametresini ayır
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_filename = argv[i + 1];
                i++; // Çıktı dosya adını geçmek için indeksi artır
            } else {
                fprintf(stderr, "Hata: -o parametresinden sonra dosya adi belirtilmedi!\n");
                exit(1);
            }
        } else {
            if (file_count < MAX_FILES) {
                input_files[file_count++] = argv[i];
            } else {
                fprintf(stderr, "Hata: Giris dosyasi sayisi en fazla 32 olabilir!\n");
                exit(1);
            }
        }
    }

    if (file_count == 0) {
        fprintf(stderr, "Hata: Arşivlenecek giriş dosyası belirtilmedi!\n");
        exit(1);
    }

    // Giriş dosyalarının boyut, varlık ve ASCII uyumluluk kontrollerini yap
    unsigned long total_size = 0;
    struct stat file_stat;
    
    for (int i = 0; i < file_count; i++) {
        if (stat(input_files[i], &file_stat) < 0) {
            fprintf(stderr, "Hata: %s dosyasi bulunamadi!\n", input_files[i]);
            exit(1);
        }

        total_size += file_stat.st_size;
        if (total_size > MAX_TOTAL_SIZE) {
            fprintf(stderr, "Hata: Giriş dosyalarının toplam boyutu 200 MB'ı geçemez!\n");
            exit(1);
        }

        // ASCII kontrolü - Uyumsuzsa dokümanda istenen spesifik hata mesajını bas ve çık
        if (!is_ascii_file(input_files[i])) {
            printf("%s giriş dosyasının formatı uyumsuzdur!\n", input_files[i]);
            exit(0); // Doküman kuralı: sorunsuz bir şekilde programdan çıkılmalıdır.
        }
    }

    // Organizasyon (İçerik) bölümünü oluştur: |dosya_adı,izinler,boyut|
    char metadata[10000] = ""; 
    for (int i = 0; i < file_count; i++) {
        stat(input_files[i], &file_stat);
        char file_info[512];
        // İzinleri oktal (sekizlik) tabanda (%o) kaydediyoruz
        sprintf(file_info, "|%s,%o,%ld|", input_files[i], file_stat.st_mode & 0777, file_stat.st_size);
        strcat(metadata, file_info);
    }

    // İlk bölümün toplam boyutunu hesapla (Kendi 10 baytlık boyutu + metadata uzunluğu)
    long metadata_section_size = strlen(metadata) + 10;
    
    // Arşiv dosyasını yazma modunda aç
    FILE *archive = fopen(output_filename, "wb");
    if (!archive) {
        fprintf(stderr, "Hata: Çıktı dosyası oluşturulamadı!\n");
        exit(1);
    }

    // İlk 10 bayta organizasyon bölümünün boyutunu soluna sıfır doldurarak yaz (Örn: 0000000085)
    fprintf(archive, "%010ld", metadata_section_size);
    // Organizasyon verisini yaz
    fprintf(archive, "%s", metadata);

    // Dosya içeriklerini (Veri Bölümü) ardışık olarak arşive ekle
    for (int i = 0; i < file_count; i++) {
        FILE *infile = fopen(input_files[i], "rb");
        if (infile) {
            int ch;
            while ((ch = fgetc(infile)) != EOF) {
                fputc(ch, archive);
            }
            fclose(infile);
        }
    }

    fclose(archive);
    printf("Dosyalar birleştirildi.\n");
}

/* * -a Parametresi: .sau arşiv dosyasını çözer, içerisindeki dosyaları izinleriyle geri çıkartır
 * ve kılavuzda istendiği gibi isimlerini listeleyerek ekrana basar.
 */
void extract_files(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        printf("Arşiv dosyası uygunsuz veya bozuk!\n");
        exit(0);
    }

    char *archive_filename = argv[2];
    char *dest_dir = (argc == 4) ? argv[3] : NULL;

    char *ext = strrchr(archive_filename, '.');
    if (!ext || strcmp(ext, ".sau") != 0) {
        printf("Arşiv dosyası uygunsuz veya bozuk!\n");
        exit(0);
    }

    FILE *archive = fopen(archive_filename, "rb");
    if (!archive) {
        printf("Arşiv dosyası uygunsuz veya bozuk!\n");
        exit(0);
    }

    char size_buf[11] = {0};
    if (fread(size_buf, 1, 10, archive) != 10) {
        printf("Arşiv dosyası uygunsuz veya bozuk!\n");
        fclose(archive);
        exit(0);
    }
    long metadata_section_size = atol(size_buf);

    long metadata_data_size = metadata_section_size - 10;
    char *metadata = malloc(metadata_data_size + 1);
    // (size_t) dönüşümü yapılarak Warning tamamen engellendi
    if (fread(metadata, 1, metadata_data_size, archive) != (size_t)metadata_data_size) {
        printf("Arşiv dosyası uygunsuz veya bozuk!\n");
        free(metadata);
        fclose(archive);
        exit(0);
    }
    metadata[metadata_data_size] = '\0';

    if (dest_dir != NULL) {
        struct stat st = {0};
        if (stat(dest_dir, &st) == -1) {
            if (mkdir(dest_dir, 0777) == -1) {
                fprintf(stderr, "Hata: Hedef dizin oluşturulamadı!\n");
                free(metadata);
                fclose(archive);
                exit(1);
            }
        }
    }

    // Açılan dosyaların isimlerini hafızada tutmak için dinamik liste yapısı
    char extracted_names[MAX_FILES][256];
    int extracted_count = 0;

    char *p = metadata;
    while (*p != '\0') {
        if (*p == '|') {
            p++;
            if (*p == '\0' || *p == '|') continue;

            char record[512] = {0};
            int idx = 0;
            while (*p != '|' && *p != '\0' && idx < 511) {
                record[idx++] = *p++;
            }

            if (idx > 0) {
                char fname[256], perm_str[32], size_str[32];
                
                char *comma1 = strchr(record, ',');
                if (!comma1) continue;
                *comma1 = '\0';
                strcpy(fname, record);
                
                char *comma2 = strchr(comma1 + 1, ',');
                if (!comma2) continue;
                *comma2 = '\0';
                strcpy(perm_str, comma1 + 1);
                strcpy(size_str, comma2 + 1);

                long fsize = atol(size_str);
                mode_t fmode = strtol(perm_str, NULL, 8);

                // İsmi listeye kaydet
                if (extracted_count < MAX_FILES) {
                    strcpy(extracted_names[extracted_count++], fname);
                }

                char final_path[512];
                if (dest_dir != NULL) {
                    snprintf(final_path, sizeof(final_path), "%s/%s", dest_dir, fname);
                } else {
                    snprintf(final_path, sizeof(final_path), "%s", fname);
                }

                FILE *outfile = fopen(final_path, "wb");
                if (!outfile) {
                    fprintf(stderr, "Hata: Çıkış dosyası %s oluşturulamadı!\n", final_path);
                    continue;
                }

                for (long i = 0; i < fsize; i++) {
                    int ch = fgetc(archive);
                    if (ch != EOF) {
                        fputc(ch, outfile);
                    }
                }
                fclose(outfile);
                chmod(final_path, fmode);
            }
        } else {
            p++;
        }
    }

    free(metadata);
    fclose(archive);
    
    // Kılavuzdaki çıktı formatını oluşturma: "t1, t2, t3 ve t4.txt dosyaları açıldı."
    char summary[4096] = "";
    for (int i = 0; i < extracted_count; i++) {
        strcat(summary, extracted_names[i]);
        if (i < extracted_count - 2) {
            strcat(summary, ", ");
        } else if (i == extracted_count - 2) {
            strcat(summary, " ve ");
        }
    }

    if (dest_dir != NULL) {
        printf("%s dizininde %s dosyaları açıldı.\n", dest_dir, summary);
    } else {
        printf("%s dosyaları açıldı.\n", summary);
    }
}

/*
 * Ana Fonksiyon: Programın başlangıç noktası. Parametre kontrolü yapar.
 */
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Hata: Eksik parametre!\n");
        return 1;
    }

    if (strcmp(argv[1], "-b") == 0) {
        archive_files(argc, argv);
    } else if (strcmp(argv[1], "-a") == 0) {
        extract_files(argc, argv);
    } else {
        fprintf(stderr, "Hata: Gecersiz parametre!\n");
        return 1;
    }

    return 0;
}