/*
 * csv_io.c - CSV import/export implementation
 */

#include "csv_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>

#define MAX_LINE_LEN 1024

/* Locale-independent atof (handles both '.' and ',' as decimal separator) */
static double parse_double(const char *str)
{
    double result = 0.0;
    double fraction = 0.0;
    double divisor = 1.0;
    int negative = 0;
    int in_fraction = 0;

    /* Skip whitespace */
    while (isspace((unsigned char)*str)) str++;

    /* Handle sign */
    if (*str == '-') { negative = 1; str++; }
    else if (*str == '+') { str++; }

    /* Parse digits */
    while (*str) {
        if (*str >= '0' && *str <= '9') {
            if (in_fraction) {
                divisor *= 10.0;
                fraction = fraction * 10.0 + (*str - '0');
            } else {
                result = result * 10.0 + (*str - '0');
            }
        } else if (*str == '.' || *str == ',') {
            in_fraction = 1;
        } else {
            break;  /* Stop at first non-numeric character */
        }
        str++;
    }

    result += fraction / divisor;
    return negative ? -result : result;
}

/* Trim whitespace from both ends of a string */
static char *trim(char *str)
{
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    return str;
}

/* Parse a CSV line into fields (handles simple cases, no quoted commas) */
static int parse_csv_line(char *line, char **fields, int max_fields)
{
    int count = 0;
    char *token = strtok(line, ",;");

    while (token && count < max_fields) {
        fields[count++] = trim(token);
        token = strtok(NULL, ",;");
    }

    return count;
}

int csv_load_pieces(const char *filename, CSPInstance *instance)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Erreur: Impossible d'ouvrir %s\n", filename);
        return -1;
    }

    /* Auto-detect Odoo format by checking first line */
    char first_line[2048];
    if (fgets(first_line, sizeof(first_line), fp)) {
        /* Odoo format has "Source" header or "Quantité à produire" */
        if (strstr(first_line, "Source") && strstr(first_line, "Quantit")) {
            fclose(fp);
            return csv_load_pieces_odoo(filename, instance);
        }
    }
    rewind(fp);

    char line[MAX_LINE_LEN];
    char *fields[16];
    int line_num = 0;
    int loaded = 0;
    bool header_skipped = false;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        /* Skip empty lines and comments */
        char *trimmed = trim(line);
        if (trimmed[0] == '\0' || trimmed[0] == '#') continue;

        /* Skip header line (first non-comment line with "label" or "longueur") */
        if (!header_skipped && (strstr(trimmed, "label") || strstr(trimmed, "longueur"))) {
            header_skipped = true;
            continue;
        }

        int nfields = parse_csv_line(line, fields, 16);
        if (nfields < 2) {
            fprintf(stderr, "Attention: Ligne %d ignoree (format invalide)\n", line_num);
            continue;
        }

        /* Parse fields: label, length, quantity, [diameter, thickness] */
        const char *label = fields[0];
        double length = atof(fields[1]);
        int quantity = (nfields >= 3) ? atoi(fields[2]) : 1;
        double diameter = (nfields >= 4) ? atof(fields[3]) : 0;
        double thickness = (nfields >= 5) ? atof(fields[4]) : 0;

        if (length <= 0) {
            fprintf(stderr, "Attention: Ligne %d ignoree (longueur invalide)\n", line_num);
            continue;
        }

        if (quantity <= 0) quantity = 1;

        int id;
        if (diameter > 0 || thickness > 0) {
            id = csp_add_piece_tube(instance, length, diameter, thickness, quantity,
                                    MATERIAL_UNKNOWN, label);
        } else {
            id = csp_add_piece(instance, length, quantity, label);
        }
        if (id < 0) {
            fprintf(stderr, "Erreur: Nombre maximum de pieces atteint\n");
            break;
        }
        loaded++;
    }

    fclose(fp);
    return loaded;
}

/* Parse a quoted CSV field, returns pointer to content after closing quote */
static const char *parse_quoted_field(const char *start, char *out, size_t max_len)
{
    /* Skip leading whitespace */
    while (*start == ' ' || *start == '\t') start++;

    size_t i = 0;

    /* Check if field is quoted */
    if (*start == '"') {
        start++;
        /* Parse quoted field */
        while (*start && i < max_len - 1) {
            if (*start == '"') {
                if (*(start + 1) == '"') {
                    /* Escaped quote */
                    out[i++] = '"';
                    start += 2;
                } else {
                    /* End of quoted field */
                    start++;
                    break;
                }
            } else {
                out[i++] = *start++;
            }
        }
    } else {
        /* Parse unquoted field - read until comma, semicolon or end of line */
        while (*start && *start != ',' && *start != ';' && *start != '\n' && *start != '\r' && i < max_len - 1) {
            out[i++] = *start++;
        }
        /* Trim trailing whitespace */
        while (i > 0 && (out[i-1] == ' ' || out[i-1] == '\t')) {
            i--;
        }
    }

    out[i] = '\0';

    /* Skip comma/semicolon separator and trailing whitespace */
    while (*start == ',' || *start == ';' || *start == ' ' || *start == '\t') start++;
    return start;
}

/* Find the LAST occurrence of Ø (UTF-8: C3 98) in a string */
static const char *find_last_diameter_symbol(const char *str)
{
    const char *last = NULL;
    const char *p = str;
    while (*p) {
        if ((unsigned char)p[0] == 0xC3 &&
            ((unsigned char)p[1] == 0x98 || (unsigned char)p[1] == 0xB8)) {
            last = p;
            p += 2;
        } else {
            p++;
        }
    }
    return last;
}

/* Extract double value from a string pattern like "Ø 60x2.5 lg: 500mm" */
static int extract_dimensions_from_last(const char *str, double *diameter, double *thickness, double *length)
{
    /* Find the LAST Ø */
    const char *p = find_last_diameter_symbol(str);
    if (!p) return 0;

    p += 2;  /* Skip the 2-byte UTF-8 Ø character */

    /* Skip whitespace */
    while (*p == ' ' || *p == '\t') p++;

    /* Parse diameter */
    *diameter = parse_double(p);
    if (*diameter <= 0) return 0;

    /* Skip to 'x' separator */
    while (*p && *p != 'x' && *p != 'X') p++;
    if (!*p) return 0;
    p++;  /* Skip 'x' */

    /* Parse thickness (between x and " lg:") */
    *thickness = parse_double(p);

    /* Find " lg:" or " lg: " */
    const char *lg = strstr(p, " lg:");
    if (!lg) lg = strstr(p, "lg:");
    if (lg) {
        lg += 4;  /* Skip "lg:" */
        while (*lg == ' ') lg++;
        *length = parse_double(lg);
    } else {
        *length = 0;
    }

    return (*diameter > 0) ? 1 : 0;
}

/* Extract material type from the word BEFORE the last Ø */
static MaterialType extract_material_before_last_diameter(const char *str)
{
    const char *last_diam = find_last_diameter_symbol(str);
    if (!last_diam) return MATERIAL_UNKNOWN;

    /* Search backwards from the Ø to find material keyword */
    /* Look in the 50 characters before Ø */
    const char *search_start = (last_diam - str > 50) ? last_diam - 50 : str;

    /* Create a substring to search in */
    char buf[64];
    size_t len = last_diam - search_start;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    strncpy(buf, search_start, len);
    buf[len] = '\0';

    /* Case-insensitive search for material keywords */
    for (char *p = buf; *p; p++) {
        /* Check for "epoxy" */
        if ((*p == 'E' || *p == 'e') &&
            (p[1] == 'P' || p[1] == 'p') &&
            (p[2] == 'O' || p[2] == 'o') &&
            (p[3] == 'X' || p[3] == 'x') &&
            (p[4] == 'Y' || p[4] == 'y')) {
            return MATERIAL_EPOXY;
        }
        /* Check for "alu" */
        if ((*p == 'A' || *p == 'a') &&
            (p[1] == 'L' || p[1] == 'l') &&
            (p[2] == 'U' || p[2] == 'u')) {
            return MATERIAL_ALU;
        }
    }
    return MATERIAL_UNKNOWN;
}

/* Extract label from Odoo source field according to rules:
 * - If "comprenant" present: between first "]" and "comprenant"
 * - If "comprenant" absent: between first space and first "/"
 * - Strip "Ensemble jaumière " prefix if present
 */
static void extract_odoo_label(const char *source, char *label, size_t max_len)
{
    const char *comprenant = strstr(source, "comprenant");

    if (comprenant) {
        /* Find first "]" */
        const char *bracket = strchr(source, ']');
        if (bracket && bracket < comprenant) {
            bracket++;  /* Skip ']' */
            /* Skip whitespace */
            while (*bracket == ' ') bracket++;
            /* Skip "Ensemble jaumière " prefix if present */
            const char *ensemble = "Ensemble jaumière ";
            size_t prefix_len = strlen(ensemble);
            if (strncmp(bracket, ensemble, prefix_len) == 0) {
                bracket += prefix_len;
            }
            /* Copy until "comprenant" */
            size_t len = comprenant - bracket;
            /* Trim trailing spaces */
            while (len > 0 && bracket[len-1] == ' ') len--;
            if (len >= max_len) len = max_len - 1;
            strncpy(label, bracket, len);
            label[len] = '\0';
            return;
        }
    }

    /* No "comprenant": between first space and first "/" */
    const char *first_space = strchr(source, ' ');
    if (first_space) {
        first_space++;  /* Skip space */
        const char *first_slash = strchr(first_space, '/');
        if (first_slash) {
            size_t len = first_slash - first_space;
            /* Trim trailing spaces */
            while (len > 0 && first_space[len-1] == ' ') len--;
            if (len >= max_len) len = max_len - 1;
            strncpy(label, first_space, len);
            label[len] = '\0';
            return;
        }
    }

    /* Fallback: use the (truncated) source as the label */
    snprintf(label, max_len, "%s", source);
}

int csv_load_pieces_odoo(const char *filename, CSPInstance *instance)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Erreur: Impossible d'ouvrir %s\n", filename);
        return -1;
    }

    char line[2048];  /* Odoo lines can be long */
    int line_num = 0;
    int loaded = 0;
    bool header_skipped = false;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        /* Skip empty lines */
        char *trimmed = trim(line);
        if (trimmed[0] == '\0' || trimmed[0] == '#') continue;

        /* Skip header line */
        if (!header_skipped) {
            if (strstr(trimmed, "Source") || strstr(trimmed, "Quantité")) {
                header_skipped = true;
                continue;
            }
        }

        /* Parse quoted fields */
        char source[1024] = "";
        char qty_str[64] = "";
        const char *p = trimmed;

        p = parse_quoted_field(p, source, sizeof(source));
        if (!p || source[0] == '\0') {
            fprintf(stderr, "Attention: Ligne %d ignoree (format invalide)\n", line_num);
            continue;
        }

        parse_quoted_field(p, qty_str, sizeof(qty_str));
        int quantity = (int)atof(qty_str);
        if (quantity <= 0) quantity = 1;

        /* Extract data from source field using LAST Ø occurrence */
        double diameter = 0, thickness = 0, length = 0;
        extract_dimensions_from_last(source, &diameter, &thickness, &length);

        if (length <= 0) {
            fprintf(stderr, "Attention: Ligne %d ignoree (longueur non trouvee)\n", line_num);
            continue;
        }

        /* Material from word before LAST Ø */
        MaterialType material = extract_material_before_last_diameter(source);

        /* Extract label according to rules */
        char label[MAX_LABEL_LEN];
        extract_odoo_label(source, label, sizeof(label));

        /* Add to instance (merges with existing if same dimensions + material) */
        int id;
        if (diameter > 0 || thickness > 0) {
            id = csp_add_piece_tube(instance, length, diameter, thickness, quantity,
                                    material, label);
        } else {
            id = csp_add_piece(instance, length, quantity, label);
        }
        if (id < 0) {
            fprintf(stderr, "Erreur: Nombre maximum de pieces atteint\n");
            break;
        }

        loaded++;
    }

    fclose(fp);
    return loaded;
}

int csv_load_stock(const char *filename, CSPInstance *instance)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Erreur: Impossible d'ouvrir %s\n", filename);
        return -1;
    }

    char line[MAX_LINE_LEN];
    char *fields[16];
    int line_num = 0;
    int loaded = 0;
    bool header_skipped = false;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        /* Skip empty lines and comments */
        char *trimmed = trim(line);
        if (trimmed[0] == '\0' || trimmed[0] == '#') continue;

        /* Skip header line (first non-comment line with "label" or "longueur") */
        if (!header_skipped && (strstr(trimmed, "label") || strstr(trimmed, "longueur"))) {
            header_skipped = true;
            continue;
        }

        int nfields = parse_csv_line(line, fields, 16);
        if (nfields < 2) {
            fprintf(stderr, "Attention: Ligne %d ignoree (format invalide)\n", line_num);
            continue;
        }

        /* Parse fields: label, length, quantity, cost, [diameter, thickness] */
        const char *label = fields[0];
        double length = atof(fields[1]);
        int quantity = (nfields >= 3) ? atoi(fields[2]) : -1;  /* -1 = unlimited */
        double cost = (nfields >= 4) ? atof(fields[3]) : 1.0;
        double diameter = (nfields >= 5) ? atof(fields[4]) : 0;
        double thickness = (nfields >= 6) ? atof(fields[5]) : 0;

        if (length <= 0) {
            fprintf(stderr, "Attention: Ligne %d ignoree (longueur invalide)\n", line_num);
            continue;
        }

        if (cost <= 0) cost = 1.0;

        int id;
        if (diameter > 0 || thickness > 0) {
            id = csp_add_stock_tube(instance, length, diameter, thickness, quantity, cost, label);
        } else {
            id = csp_add_stock(instance, length, quantity, cost, label);
        }
        if (id < 0) {
            fprintf(stderr, "Erreur: Nombre maximum de stocks atteint\n");
            break;
        }
        loaded++;
    }

    fclose(fp);
    return loaded;
}

