#!/usr/bin/env python3
"""
Export stock from CSV to ODS file with formulas
"""

import sys
import csv
from odf.opendocument import OpenDocumentSpreadsheet
from odf.style import Style, TableColumnProperties, TableRowProperties, TableCellProperties, TextProperties
from odf.number import NumberStyle, Number, Text as NumberText
from odf.text import P
from odf.table import Table, TableColumn, TableRow, TableCell
from odf.namespaces import TABLENS

def create_styles(doc):
    """Create styles for the spreadsheet"""
    # Header style
    header_style = Style(name="HeaderStyle", family="table-cell")
    header_style.addElement(TableCellProperties(backgroundcolor="#4472C4"))
    header_style.addElement(TextProperties(color="#FFFFFF", fontweight="bold"))
    doc.automaticstyles.addElement(header_style)

    # Number style with 2 decimal places
    number_style = NumberStyle(name="NumStyle1")
    number_style.addElement(Number(decimalplaces="2", minintegerdigits="1", grouping="true"))
    doc.styles.addElement(number_style)

    # Cell style for cost column (uses the number format)
    cost_style = Style(name="CostStyle", family="table-cell", datastylename="NumStyle1")
    doc.automaticstyles.addElement(cost_style)

    return header_style, cost_style

def main():
    if len(sys.argv) != 3:
        print("Usage: ods_export.py <input.csv> <output.ods>", file=sys.stderr)
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    try:
        # Debug: print CSV content
        print(f"DEBUG: Reading CSV from {input_file}")
        with open(input_file, 'r') as f:
            print("CSV header:", f.readline().strip())
            for i, line in enumerate(f):
                if i < 3:  # Print first 3 data rows
                    print(f"Row {i+1}:", line.strip())

        # Create ODS document
        doc = OpenDocumentSpreadsheet()
        header_style, cost_style = create_styles(doc)

        # Create table
        table = Table(name="stock_odoo")

        # Add columns (9 columns)
        for _ in range(9):
            table.addElement(TableColumn())

        # Header row
        header_row = TableRow()
        headers = ['ID', 'Label', 'Matière', 'Longueur', 'Diamètre', 'Epaisseur', 'Quantité', 'Coût', 'Type']
        for header_text in headers:
            cell = TableCell(stylename=header_style, valuetype="string")
            cell.addElement(P(text=header_text))
            header_row.addElement(cell)
        table.addElement(header_row)

        # Read CSV and add data rows
        with open(input_file, 'r') as f:
            reader = csv.DictReader(f)
            row_num = 2  # Start at row 2 (after header)
            count = 0

            for row in reader:
                data_row = TableRow()

                # ID (column A)
                cell = TableCell(valuetype="float", value=row.get('id', '0'))
                cell.addElement(P(text=row.get('id', '0')))
                data_row.addElement(cell)

                # Label (column B)
                cell = TableCell(valuetype="string")
                cell.addElement(P(text=row.get('label', '')))
                data_row.addElement(cell)

                # Matière (column C)
                material = int(row.get('material', 0))
                material_str = 'Alu' if material == 1 else 'Epoxy' if material == 2 else ''
                cell = TableCell(valuetype="string")
                cell.addElement(P(text=material_str))
                data_row.addElement(cell)

                # Longueur (column D)
                length = row.get('length', '0')
                cell = TableCell(valuetype="float", value=length, )
                cell.addElement(P(text=length))
                data_row.addElement(cell)

                # Diamètre (column E)
                diameter = row.get('diameter', '0')
                cell = TableCell(valuetype="float", value=diameter, )
                cell.addElement(P(text=diameter))
                data_row.addElement(cell)

                # Epaisseur (column F)
                thickness = row.get('thickness', '0')
                cell = TableCell(valuetype="float", value=thickness, )
                cell.addElement(P(text=thickness))
                data_row.addElement(cell)

                # Quantité (column G)
                quantity = row.get('quantity', '0')
                cell = TableCell(valuetype="float", value=quantity, )
                cell.addElement(P(text=quantity))
                data_row.addElement(cell)

                # Coût (column H) - WITH FORMULA
                # OpenFormula uses English function names with comma separators
                # LibreOffice will translate to local language when displaying
                # Calculate initial value for LibreOffice
                is_offcut = int(row.get('is_offcut', 0))
                length = float(row.get('length', 0))
                initial_value = 1.0 if not is_offcut else length / 6000.0

                formula_text = f'of:=IF([.I{row_num}]="Stock";1;[.D{row_num}]/6000)'
                # Pass formula with initial calculated value and cost style (2 decimals)
                cell = TableCell(valuetype="float", value=str(initial_value),
                                stylename=cost_style,
                                qattributes={(TABLENS, 'formula'): formula_text})
                cell.addElement(P(text=f"{initial_value:.2f}"))
                data_row.addElement(cell)

                # Type (column I)
                is_offcut = int(row.get('is_offcut', 0))
                type_str = 'Chute' if is_offcut else 'Stock'
                cell = TableCell(valuetype="string")
                cell.addElement(P(text=type_str))
                data_row.addElement(cell)

                table.addElement(data_row)
                row_num += 1
                count += 1

        doc.spreadsheet.addElement(table)
        doc.save(output_file)

        print(f"Exported {count} stock items with formulas")

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == '__main__':
    main()
