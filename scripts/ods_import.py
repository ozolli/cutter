#!/usr/bin/env python3
"""
Import stock from ODS file to CSV
"""

import sys
import pandas as pd

def main():
    if len(sys.argv) != 3:
        print("Usage: ods_import.py <input.ods> <output.csv>", file=sys.stderr)
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    try:
        # Read ODS file
        df = pd.read_excel(input_file, engine='odf', sheet_name=0)

        print(f"DEBUG: Read {len(df)} rows from ODS")
        print(f"DEBUG: Columns: {list(df.columns)}")
        if len(df) > 0:
            print(f"DEBUG: First row: {df.iloc[0].to_dict()}")

        # Expected columns: ID, Label, Matière, Longueur, Diamètre, Epaisseur, Quantité, Coût, Type
        # Convert to CSV format: label,length,diameter,thickness,quantity,cost,material,is_offcut

        result = []
        for idx, row in df.iterrows():
            # Skip header row if needed
            if pd.isna(row.get('Longueur')) or row.get('Longueur') == 'Longueur':
                continue

            label = str(row['Label']) if not pd.isna(row.get('Label')) else ''
            length = float(row['Longueur']) if not pd.isna(row.get('Longueur')) else 0
            diameter = float(row['Diamètre']) if not pd.isna(row.get('Diamètre')) else 0
            thickness = float(row['Epaisseur']) if not pd.isna(row.get('Epaisseur')) else 0
            quantity = int(row['Quantité']) if not pd.isna(row.get('Quantité')) else 0

            # Material: Alu -> 1, Epoxy -> 2, other -> 0
            material_str = str(row.get('Matière', '')).strip().lower()
            if 'alu' in material_str:
                material = 1
            elif 'epoxy' in material_str:
                material = 2
            else:
                material = 0

            # Type: Stock or Chute
            type_str = str(row.get('Type', 'Stock')).strip().lower()
            is_offcut = 1 if 'chute' in type_str else 0

            # Calculate cost
            if is_offcut:
                cost = length / 6000.0
                if cost < 0.01:
                    cost = 0.01
            else:
                cost = 1.0

            result.append({
                'label': label,
                'length': length,
                'diameter': diameter,
                'thickness': thickness,
                'quantity': quantity,
                'cost': cost,
                'material': material,
                'is_offcut': is_offcut
            })

        # Write CSV
        result_df = pd.DataFrame(result)
        result_df.to_csv(output_file, index=False)

        print(f"Imported {len(result)} stock items")

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()
