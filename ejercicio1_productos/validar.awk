#!/usr/bin/awk -f
# ================================================================
# validar.awk
# ================================================================
# Valida el archivo productos.csv generado por el programa.
# Verifica que el archivo CSV generado cumpla:
#  - IDs correlativos (sin saltos)
#  - Sin IDs duplicados
#  - Reporta totales y posibles errores
#
# Uso:
#   awk -v GENS=5 -v TOTAL=100 -f validar.awk productos.csv
# ================================================================

BEGIN {
    FS = ","
    errores = 0
    advertencias = 0

    # Valores por defecto
    if (TOTAL == "" || TOTAL <= 0) TOTAL = 100
    if (GENS == ""  || GENS  <= 0) GENS = 4

    printf("Usando parámetros: TOTAL=%d, GENS=%d\n", TOTAL, GENS)
}

# Procesar encabezado
NR == 1 {
    for (i = 1; i <= NF; i++) {
        col = tolower($i)
        gsub(/^[ \t]+|[ \t]+$/, "", col)
        if (col == "generador") gen_col = i
    }
    next
}

# Ignorar comentarios y líneas vacías
/^#/ || NF < 1 { next }

{
    # Limpiar valores
    gsub(/\r/, "", $1)
    gsub(/^[ \t]+|[ \t]+$/, "", $1)

    id = $1 + 0

    # ID inválido
    if (id <= 0 || id != int(id)) {
        printf("❌ Línea %d: ID inválido (%s)\n", NR, $1)
        errores++
        next
    }

    # ID duplicado
    if (id in vistos) {
        printf("❌ Línea %d: ID duplicado (%d)\n", NR, id)
        errores++
    }
    vistos[id] = NR  # guardar línea donde se vio

    # Rango mínimo/máximo
    if (min_id == "" || id < min_id) min_id = id
    if (max_id == "" || id > max_id) max_id = id

    # Leer generador si existe
    if (gen_col > 0) {
        gen = $(gen_col) + 0
        if (gen <= 0 || gen != int(gen)) {
            printf("❌ Línea %d: Generador inválido (%s)\n", NR, $(gen_col))
            errores++
        } else {
            gens[gen]++
            if (gen > max_gen) max_gen = gen
            if (min_gen == "" || gen < min_gen) min_gen = gen
        }
    }

    # Detectar desorden (IDs no correlativos ascendentes)
    if (prev_id != "" && id < prev_id) {
        printf("❌ Línea %d: ID fuera de orden (%d después de %d)\n", NR, id, prev_id)
        errores++
    }
    prev_id = id
}

END {
    total_reg = 0
    for (i in vistos) total_reg++

    # Verificar correlatividad (faltantes en cualquier parte)
    for (i = min_id; i <= max_id; i++) {
        if (!(i in vistos)) {
            printf("❌ Falta ID %d (no encontrado en el archivo)\n", i)
            errores++
        }
    }

    print "----------------------------------------------"
    print "📊 Resumen de validación del CSV"
    print "----------------------------------------------"
    print "Total de registros leídos : " total_reg
    print "ID mínimo                 : " min_id
    print "ID máximo                 : " max_id
    print "Esperado correlativo      : " (max_id - min_id + 1)

    # Verificación de TOTAL
    if (total_reg != TOTAL) {
        printf("⚠️  Advertencia: Cantidad de registros (%d) no coincide con TOTAL esperado (%d)\n", total_reg, TOTAL)
        advertencias++
    }
    if (max_id != TOTAL) {
        printf("⚠️  Advertencia: ID máximo (%d) no coincide con TOTAL esperado (%d)\n", max_id, TOTAL)
        advertencias++
    }

    # Verificación de generadores
    if (gen_col == 0) {
        printf("⚠️  Advertencia: Columna 'Generador' no encontrada. No se puede validar GENS.\n")
        advertencias++
    } else {
        gens_contados = 0
        for (g in gens) gens_contados++
        print "Generadores detectados    : " gens_contados

        if (gens_contados != GENS) {
            printf("⚠️  Advertencia: Generadores distintos (%d) no coincide con GENS esperado (%d)\n", gens_contados, GENS)
            advertencias++
        }
        if (max_gen != GENS) {
            printf("⚠️  Advertencia: Generador máximo (%d) no coincide con GENS esperado (%d)\n", max_gen, GENS)
            advertencias++
        }
    }

    print "----------------------------------------------"

    # Resultado final
    if (errores == 0 && advertencias == 0) {
        print "✅ VALIDACIÓN EXITOSA: Todos los IDs son únicos, correlativos y ordenados."
        exit 0
    }

    if (errores == 0 && advertencias > 0) {
        print "⚠️  VALIDACIÓN EXITOSA CON ADVERTENCIAS."
        exit 0
    }

    if (errores > 0) {
        printf("❌ VALIDACIÓN FALLIDA: Se detectaron %d error(es).\n", errores)
        exit 1
    }
}
