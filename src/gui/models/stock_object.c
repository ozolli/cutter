/*
 * stock_object.c - GObject wrapper for StockItem
 */

#include "stock_object.h"
#include <string.h>

struct _StockObject {
    GObject parent_instance;

    int64_t id;
    char *label;
    double length;
    double diameter;
    double thickness;
    int32_t quantity;
    double cost;
    MaterialType material;
    gboolean is_offcut;
};

G_DEFINE_TYPE(StockObject, stock_object, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_ID,
    PROP_LABEL,
    PROP_LENGTH,
    PROP_DIAMETER,
    PROP_THICKNESS,
    PROP_QUANTITY,
    PROP_COST,
    PROP_MATERIAL,
    PROP_IS_OFFCUT,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void stock_object_finalize(GObject *object)
{
    StockObject *self = CUTTER_STOCK_OBJECT(object);
    g_free(self->label);
    G_OBJECT_CLASS(stock_object_parent_class)->finalize(object);
}

static void stock_object_get_property(GObject *object, guint prop_id,
                                       GValue *value, GParamSpec *pspec)
{
    StockObject *self = CUTTER_STOCK_OBJECT(object);

    switch (prop_id) {
        case PROP_ID:
            g_value_set_int64(value, self->id);
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
        case PROP_COST:
            g_value_set_double(value, self->cost);
            break;
        case PROP_MATERIAL:
            g_value_set_int(value, (int)self->material);
            break;
        case PROP_IS_OFFCUT:
            g_value_set_boolean(value, self->is_offcut);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void stock_object_set_property(GObject *object, guint prop_id,
                                       const GValue *value, GParamSpec *pspec)
{
    StockObject *self = CUTTER_STOCK_OBJECT(object);

    switch (prop_id) {
        case PROP_ID:
            self->id = g_value_get_int64(value);
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
        case PROP_COST:
            self->cost = g_value_get_double(value);
            break;
        case PROP_MATERIAL:
            self->material = (MaterialType)g_value_get_int(value);
            break;
        case PROP_IS_OFFCUT:
            self->is_offcut = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void stock_object_class_init(StockObjectClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = stock_object_finalize;
    object_class->get_property = stock_object_get_property;
    object_class->set_property = stock_object_set_property;

    properties[PROP_ID] = g_param_spec_int64("id", "ID", "Database ID",
                                              G_MININT64, G_MAXINT64, 0,
                                              G_PARAM_READWRITE);
    properties[PROP_LABEL] = g_param_spec_string("label", "Label", "Stock label",
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
    properties[PROP_QUANTITY] = g_param_spec_int("quantity", "Quantity", "Available quantity",
                                                  0, G_MAXINT, 0,
                                                  G_PARAM_READWRITE);
    properties[PROP_COST] = g_param_spec_double("cost", "Cost", "Cost per unit",
                                                 0, G_MAXDOUBLE, 1.0,
                                                 G_PARAM_READWRITE);
    properties[PROP_MATERIAL] = g_param_spec_int("material", "Material", "Material type",
                                                  0, 2, 0,
                                                  G_PARAM_READWRITE);
    properties[PROP_IS_OFFCUT] = g_param_spec_boolean("is-offcut", "Is Offcut", "Is this an offcut",
                                                       FALSE, G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPS, properties);
}

static void stock_object_init(StockObject *self)
{
    self->id = 0;
    self->label = g_strdup("");
    self->length = 0;
    self->diameter = 0;
    self->thickness = 0;
    self->quantity = 0;
    self->cost = 1.0;
    self->material = MATERIAL_UNKNOWN;
    self->is_offcut = FALSE;
}

StockObject *stock_object_new(const StockItem *item)
{
    StockObject *self = g_object_new(CUTTER_TYPE_STOCK_OBJECT, NULL);

    self->id = item->id;
    g_free(self->label);
    self->label = g_strdup(item->label);
    self->length = item->length;
    self->diameter = item->diameter;
    self->thickness = item->thickness;
    self->quantity = item->quantity;
    self->cost = item->cost;
    self->material = item->material;
    self->is_offcut = item->is_offcut;

    return self;
}

StockObject *stock_object_new_empty(void)
{
    return g_object_new(CUTTER_TYPE_STOCK_OBJECT, NULL);
}

int64_t stock_object_get_id(StockObject *self) { return self->id; }
const char *stock_object_get_label(StockObject *self) { return self->label; }
double stock_object_get_length(StockObject *self) { return self->length; }
double stock_object_get_diameter(StockObject *self) { return self->diameter; }
double stock_object_get_thickness(StockObject *self) { return self->thickness; }
int32_t stock_object_get_quantity(StockObject *self) { return self->quantity; }
double stock_object_get_cost(StockObject *self) { return self->cost; }
MaterialType stock_object_get_material(StockObject *self) { return self->material; }
gboolean stock_object_get_is_offcut(StockObject *self) { return self->is_offcut; }

void stock_object_set_label(StockObject *self, const char *label)
{
    g_free(self->label);
    self->label = g_strdup(label);
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_LABEL]);
}

void stock_object_set_length(StockObject *self, double length)
{
    self->length = length;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_LENGTH]);
}

void stock_object_set_diameter(StockObject *self, double diameter)
{
    self->diameter = diameter;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_DIAMETER]);
}

void stock_object_set_thickness(StockObject *self, double thickness)
{
    self->thickness = thickness;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_THICKNESS]);
}

void stock_object_set_quantity(StockObject *self, int32_t quantity)
{
    self->quantity = quantity;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_QUANTITY]);
}

void stock_object_set_cost(StockObject *self, double cost)
{
    self->cost = cost;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_COST]);
}

void stock_object_set_material(StockObject *self, MaterialType material)
{
    self->material = material;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_MATERIAL]);
}

void stock_object_set_is_offcut(StockObject *self, gboolean is_offcut)
{
    self->is_offcut = is_offcut;
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_IS_OFFCUT]);
}

void stock_object_to_item(StockObject *self, StockItem *out)
{
    out->id = self->id;
    strncpy(out->label, self->label, MAX_LABEL_LEN - 1);
    out->label[MAX_LABEL_LEN - 1] = '\0';
    out->length = self->length;
    out->diameter = self->diameter;
    out->thickness = self->thickness;
    out->quantity = self->quantity;
    out->cost = self->cost;
    out->material = self->material;
    out->is_offcut = self->is_offcut;
}
