/*
 * piece_object.h - GObject wrapper for PieceDemand
 */

#ifndef PIECE_OBJECT_H
#define PIECE_OBJECT_H

#include <glib-object.h>
#include "../../csp_types.h"

G_BEGIN_DECLS

#define CUTTER_TYPE_PIECE_OBJECT (piece_object_get_type())
G_DECLARE_FINAL_TYPE(PieceObject, piece_object, CUTTER, PIECE_OBJECT, GObject)

PieceObject *piece_object_new(const PieceDemand *piece);
PieceObject *piece_object_new_empty(void);

/* Getters */
int32_t piece_object_get_id(PieceObject *self);
const char *piece_object_get_label(PieceObject *self);
double piece_object_get_length(PieceObject *self);
double piece_object_get_diameter(PieceObject *self);
double piece_object_get_thickness(PieceObject *self);
int32_t piece_object_get_quantity(PieceObject *self);
MaterialType piece_object_get_material(PieceObject *self);

/* Setters */
void piece_object_set_label(PieceObject *self, const char *label);
void piece_object_set_length(PieceObject *self, double length);
void piece_object_set_diameter(PieceObject *self, double diameter);
void piece_object_set_thickness(PieceObject *self, double thickness);
void piece_object_set_quantity(PieceObject *self, int32_t quantity);
void piece_object_set_material(PieceObject *self, MaterialType material);

/* Convert to PieceDemand struct */
void piece_object_to_piece(PieceObject *self, PieceDemand *out);

G_END_DECLS

#endif /* PIECE_OBJECT_H */
