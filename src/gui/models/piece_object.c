/*
 * piece_object.c - GObject wrapper for PieceDemand
 */

#include "piece_object.h"
#include <string.h>

struct _PieceObject {
    GObject parent_instance;

    int32_t id;
    char *label;
    double length;
    double diameter;
    double thickness;
    int32_t quantity;
    MaterialType material;
};

G_DEFINE_TYPE(PieceObject, piece_object, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_ID,
    PROP_LABEL,
    PROP_LENGTH,
    PROP_DIAMETER,
    PROP_THICKNESS,
    PROP_QUANTITY,
    PROP_MATERIAL,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void piece_object_finalize(GObject *object)
{
    PieceObject *self = CUTTER_PIECE_OBJECT(object);
    g_free(self->label);
    G_OBJECT_CLASS(piece_object_parent_class)->finalize(object);
}

static void piece_object_get_property(GObject *object, guint prop_id,
                                       GValue *value, GParamSpec *pspec)
{
    PieceObject *self = CUTTER_PIECE_OBJECT(object);

    switch (prop_id) {
        case PROP_ID:
            g_value_set_int(value, self->id);
            break;
        case PROP_LABEL:
            g_value_set_string(value, self->label);
            break;
        case PROP_LENGTH:
            g_value_set_double(value, self->length);
            break;
        case PROP_DIAMETER:
            g_value_set_double(value, self->diameter);
            break;
        case PROP_THICKNESS:
            g_value_set_double(value, self->thickness);
            break;
        case PROP_QUANTITY:
            g_value_set_int(value, self->quantity);
            break;
        case PROP_MATERIAL:
            g_value_set_int(value, (int)self->material);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void piece_object_set_property(GObject *object, guint prop_id,
                                       const GValue *value, GParamSpec *pspec)
{
    PieceObject *self = CUTTER_PIECE_OBJECT(object);

    switch (prop_id) {
        case PROP_ID:
            self->id = g_value_get_int(value);
            break;
        case PROP_LABEL:
            g_free(self->label);
            self->label = g_value_dup_string(value);
            break;
        case PROP_LENGTH:
            self->length = g_value_get_double(value);
            break;
        case PROP_DIAMETER:
            self->diameter = g_value_get_double(value);
            break;
        case PROP_THICKNESS:
            self->thickness = g_value_get_double(value);
            break;
        case PROP_QUANTITY:
            self->quantity = g_value_get_int(value);
            break;
        case PROP_MATERIAL:
            self->material = (MaterialType)g_value_get_int(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void piece_object_class_init(PieceObjectClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = piece_object_finalize;
    object_class->get_property = piece_object_get_property;
    object_class->set_property = piece_object_set_property;

    properties[PROP_ID] = g_param_spec_int("id", "ID", "Piece ID",
                                            0, G_MAXINT, 0,
                                            G_PARAM_READWRITE);
    properties[PROP_LABEL] = g_param_spec_string("label", "Label", "Piece label",
                                                  "", G_PARAM_READWRITE);
    properties[PROP_LENGTH] = g_param_spec_double("length", "Length", "Length in mm",
                                                   0, G_MAXDOUBLE, 0,
                                                   G_PARAM_READWRITE);
    properties[PROP_DIAMETER] = g_param_spec_double("diameter", "Diameter", "Diameter in mm",
                                                     0, G_MAXDOUBLE, 0,
                                                     G_PARAM_READWRITE);
    properties[PROP_THICKNESS] = g_param_spec_double("thickness", "Thickness", "Thickness in mm",
                                                      0, G_MAXDOUBLE, 0,
                                                      G_PARAM_READWRITE);
    properties[PROP_QUANTITY] = g_param_spec_int("quantity", "Quantity", "Quantity needed",
                                                  0, G_MAXINT, 1,
                                                  G_PARAM_READWRITE);
    properties[PROP_MATERIAL] = g_param_spec_int("material", "Material", "Material type",
                                                  0, 2, 0,
                                                  G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPS, properties);
}

static void piece_object_init(PieceObject *self)
{
    self->id = 0;
    self->label = g_strdup("");
    self->length = 0;
    self->diameter = 0;
    self->thickness = 0;
    self->quantity = 1;
    self->material = MATERIAL_UNKNOWN;
}

PieceObject *piece_object_new(const PieceDemand *piece)
{
    PieceObject *self = g_object_new(CUTTER_TYPE_PIECE_OBJECT, NULL);

    self->id = piece->id;
    g_free(self->label);
    self->label = g_strdup(piece->label);
    self->length = piece->length;
    self->diameter = piece->diameter;
    self->thickness = piece->thickness;
    self->quantity = piece->quantity;
    self->material = piece->material;

    return self;
}

PieceObject *piece_object_new_empty(void)
{
    return g_object_new(CUTTER_TYPE_PIECE_OBJECT, NULL);
}

int32_t piece_object_get_id(PieceObject *self) { return self->id; }
const char *piece_object_get_label(PieceObject *self) { return self->label; }
double piece_object_get_length(PieceObject *self) { return self->length; }
double piece_object_get_diameter(PieceObject *self) { return self->diameter; }
double piece_object_get_thickness(PieceObject *self) { return self->thickness; }
int32_t piece_object_get_quantity(PieceObject *self) { return self->quantity; }
MaterialType piece_object_get_material(PieceObject *self) { return self->material; }

void piece_object_set_label(PieceObject *self, const char *label)
{
    g_free(self->label);
    self->label = g_strdup(label);
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_LABEL]);
}

void piece_object_set_length(PieceObject *self, double length)
{
    self->length = length;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_LENGTH]);
}

void piece_object_set_diameter(PieceObject *self, double diameter)
{
    self->diameter = diameter;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_DIAMETER]);
}

void piece_object_set_thickness(PieceObject *self, double thickness)
{
    self->thickness = thickness;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_THICKNESS]);
}

void piece_object_set_quantity(PieceObject *self, int32_t quantity)
{
    self->quantity = quantity;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_QUANTITY]);
}

void piece_object_set_material(PieceObject *self, MaterialType material)
{
    self->material = material;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_MATERIAL]);
}

void piece_object_to_piece(PieceObject *self, PieceDemand *out)
{
    out->id = self->id;
    strncpy(out->label, self->label, MAX_LABEL_LEN - 1);
    out->label[MAX_LABEL_LEN - 1] = '\0';
    out->length = self->length;
    out->diameter = self->diameter;
    out->thickness = self->thickness;
    out->quantity = self->quantity;
    out->material = self->material;
}
