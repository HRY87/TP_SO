# validar.awk
# Uso: awk -v total=N -f validar.awk salida.csv
#   donde N es el total de registros esperado

BEGIN {
    FS=","; 
    duplicados = 0;
}

NR==1 { next }  # saltar la cabecera

{
    id = $1 + 0
    if (seen[id]++) {
        print "Error: ID duplicado encontrado en línea " NR ": " id
        duplicados++
    }
}

END {
    faltan = 0
    for (i = 1; i <= total; i++) {
        if (!(i in seen)) {
            print "Error: falta el ID " i
            faltan++
        }
    }

    if (duplicados == 0 && faltan == 0) {
        print "OK: IDs consecutivos del 1 al " total ", sin duplicados."
    } else {
        print "Total duplicados: " duplicados
        print "Total faltantes: " faltan
    }
}
