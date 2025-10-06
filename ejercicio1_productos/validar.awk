#!/usr/bin/awk -f
# ================================================================
# validar.awk
# ================================================================
# Valida el archivo productos.csv generado por el programa.
# Comprueba:
#   - Que los IDs sean únicos y consecutivos dentro del total esperado.
#   - Que cada generador (G1, G2, ...) tenga secuencias internas coherentes.
#   - Que no falten ni sobren registros.
#
# Uso:
#   awk -v GENS=2 -v TOTAL=50 -f validar.awk productos.csv
# ================================================================

BEGIN {
    FS = ","
    total_ids = 0
    errores = 0
    delete ids_vistos
    delete ult_num_gen
    delete nums_gen
}

NR == 1 {
    next  # saltar encabezado
}

{
    id = $1
    desc = $2
    gen = ""
    num = ""

    # Extraer nombre del generador (G1, G2, ...) y número interno
    if (match(desc, /^G([0-9]+)_([0-9]+)/, arr)) {
        gen = "G" arr[1]
        num = arr[2] + 0
    } else {
        print "Descripción inválida en línea " NR ": " desc
        errores++
        next
    }

    # Validar ID duplicado
    if (id in ids_vistos) {
        print "ID duplicado:", id
        errores++
    }
    ids_vistos[id] = 1
    total_ids++

    # Registrar secuencia interna por generador
    nums_gen[gen, num] = 1

    # Comprobar secuencia interna
    if (gen in ult_num_gen) {
        if (num != ult_num_gen[gen] + 1) {
            print "Secuencia no correlativa en", gen ":", ult_num_gen[gen], "→", num
            errores++
        }
    }
    ult_num_gen[gen] = num
}

END {
    print "--------------------------------------------------"
    print "📊 RESULTADOS DE VALIDACIÓN"
    print "--------------------------------------------------"

    # Validar cantidad total de registros
    if (total_ids != TOTAL) {
        print "Cantidad incorrecta de registros:"
        print "   Esperado:", TOTAL, " | Obtenido:", total_ids
        errores++
    } else {
        print "Cantidad total de registros correcta (" TOTAL ")"
    }

    # Verificar que no falten IDs globales
    for (i = 1; i <= TOTAL; i++) {
        if (!(i in ids_vistos)) {
            print "Falta el ID global:", i
            errores++
        }
    }

    # Validar secuencias internas de cada generador
    print "--------------------------------------------------"
    print "🔍 Validando secuencias internas por generador..."
    for (g = 1; g <= GENS; g++) {
        nombre = "G" g
        falta = 0
        for (i = 1; i <= TOTAL; i++) {
            if (nums_gen[nombre, i]) continue
            else if (i <= ult_num_gen[nombre]) {
                print "Falta número " i " en " nombre
                falta = 1
                errores++
            }
        }
        if (!falta && (nombre in ult_num_gen))
            print "Secuencia de " nombre " completa hasta", ult_num_gen[nombre]
    }

    print "--------------------------------------------------"
    if (errores == 0) {
        print "VALIDACIÓN EXITOSA: Todos los registros son correctos."
    } else {
        print "VALIDACIÓN FALLIDA: Se detectaron", errores, "errores."
    }
    print "--------------------------------------------------"
}
