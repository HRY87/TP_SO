#!/bin/bash
set -e
CSV="data/productos.csv"
BACKUP="data/productos.csv.bak.$(date +%s)"
FIXED="data/productos_fixed.csv"

if [ ! -f "$CSV" ]; then
  echo "No existe $CSV"
  exit 1
fi

cp "$CSV" "$BACKUP"
echo "Backup creado: $BACKUP"

awk -F',' 'BEGIN{OFS=","}
NR==1 { print $0; next } 
{
  if (NF<=6) {
    print $0
  } else {
    # primera parte: campos 1..6
    printf $1
    for(i=2;i<=6;i++) printf ","$i
    printf "\n"
    # segunda parte: campos 7..NF (se asume que forman un registro completo)
    printf $7
    for(i=8;i<=NF;i++) printf ","$i
    printf "\n"
  }
}' "$CSV" > "$FIXED"

mv "$FIXED" "$CSV"
echo "Archivo corregido: $CSV"
# asegurar newline final
sed -i -e '$a\' "$CSV"
echo "Asegurado newline final."