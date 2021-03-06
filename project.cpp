#include "asm.h"
#include "project.h"
#include "tile.h"
#include "tileset.h"
#include "metatile.h"
#include "event.h"

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QRegularExpression>

Project::Project()
{
    groupNames = new QStringList;
    groupedMapNames = new QList<QStringList*>;
    mapNames = new QStringList;
    map_cache = new QMap<QString, Map*>;
    tileset_cache = new QMap<QString, Tileset*>;
}

QString Project::getProjectTitle() {
    if (!root.isNull()) {
        return root.section('/', -1);
    } else {
        return QString();
    }
}

Map* Project::loadMap(QString map_name) {
    Map *map = new Map;

    map->name = map_name;
    readMapHeader(map);
    readMapAttributes(map);
    getTilesets(map);
    loadBlockdata(map);
    loadMapBorder(map);
    readMapEvents(map);
    loadMapConnections(map);
    map->commit();
    map->history.save();

    map_cache->insert(map_name, map);
    return map;
}

void Project::loadMapConnections(Map *map) {
    map->connections.clear();
    if (!map->connections_label.isNull()) {
        QString path = root + QString("/data/maps/%1/connections.inc").arg(map->name);
        QString text = readTextFile(path);
        if (!text.isNull()) {
            QList<QStringList> *commands = parse(text);
            QStringList *list = getLabelValues(commands, map->connections_label);

            //// Avoid using this value. It ought to be generated instead.
            //int num_connections = list->value(0).toInt(nullptr, 0);

            QString connections_list_label = list->value(1);
            QList<QStringList> *connections = getLabelMacros(commands, connections_list_label);
            for (QStringList command : *connections) {
                QString macro = command.value(0);
                if (macro == "connection") {
                    Connection *connection = new Connection;
                    connection->direction = command.value(1);
                    connection->offset = command.value(2);
                    connection->map_name = command.value(3);
                    map->connections.append(connection);
                }
            }
        }
    }
}

QList<QStringList>* Project::getLabelMacros(QList<QStringList> *list, QString label) {
    bool in_label = false;
    QList<QStringList> *new_list = new QList<QStringList>;
    for (int i = 0; i < list->length(); i++) {
        QStringList params = list->value(i);
        QString macro = params.value(0);
        if (macro == ".label") {
            if (params.value(1) == label) {
                in_label = true;
            } else if (in_label) {
                // If nothing has been read yet, assume the label
                // we're looking for is in a stack of labels.
                if (new_list->length() > 0) {
                    break;
                }
            }
        } else if (in_label) {
            new_list->append(params);
        }
    }
    return new_list;
}

// For if you don't care about filtering by macro,
// and just want all values associated with some label.
QStringList* Project::getLabelValues(QList<QStringList> *list, QString label) {
    list = getLabelMacros(list, label);
    QStringList *values = new QStringList;
    for (int i = 0; i < list->length(); i++) {
        QStringList params = list->value(i);
        QString macro = params.value(0);
        // Ignore .align
        if (macro == ".align") {
            continue;
        }
        for (int j = 1; j < params.length(); j++) {
            values->append(params.value(j));
        }
    }
    return values;
}

void Project::readMapHeader(Map* map) {
    QString label = map->name;
    Asm *parser = new Asm;

    QString header_text = readTextFile(root + "/data/maps/" + label + "/header.inc");
    if (header_text.isNull()) {
        return;
    }
    QStringList *header = getLabelValues(parser->parse(header_text), label);
    map->attributes_label = header->value(0);
    map->events_label = header->value(1);
    map->scripts_label = header->value(2);
    map->connections_label = header->value(3);
    map->song = header->value(4);
    map->index = header->value(5);
    map->location = header->value(6);
    map->visibility = header->value(7);
    map->weather = header->value(8);
    map->type = header->value(9);
    map->unknown = header->value(10);
    map->show_location = header->value(11);
    map->battle_scene = header->value(12);
}

void Project::saveMapHeader(Map *map) {
    QString label = map->name;
    QString header_path = root + "/data/maps/" + label + "/header.inc";
    QString text = "";
    text += QString("%1::\n").arg(label);
    text += QString("\t.4byte %1\n").arg(map->attributes_label);
    text += QString("\t.4byte %1\n").arg(map->events_label);
    text += QString("\t.4byte %1\n").arg(map->scripts_label);
    text += QString("\t.4byte %1\n").arg(map->connections_label);
    text += QString("\t.2byte %1\n").arg(map->song);
    text += QString("\t.2byte %1\n").arg(map->index);
    text += QString("\t.byte %1\n").arg(map->location);
    text += QString("\t.byte %1\n").arg(map->visibility);
    text += QString("\t.byte %1\n").arg(map->weather);
    text += QString("\t.byte %1\n").arg(map->type);
    text += QString("\t.2byte %1\n").arg(map->unknown);
    text += QString("\t.byte %1\n").arg(map->show_location);
    text += QString("\t.byte %1\n").arg(map->battle_scene);
    saveTextFile(header_path, text);
}

void Project::readMapAttributes(Map* map) {
    Asm *parser = new Asm;

    QString assets_text = readTextFile(root + "/data/maps/_assets.inc");
    if (assets_text.isNull()) {
        return;
    }
    QStringList *attributes = getLabelValues(parser->parse(assets_text), map->attributes_label);
    map->width = attributes->value(0);
    map->height = attributes->value(1);
    map->border_label = attributes->value(2);
    map->blockdata_label = attributes->value(3);
    map->tileset_primary_label = attributes->value(4);
    map->tileset_secondary_label = attributes->value(5);
}

void Project::getTilesets(Map* map) {
    map->tileset_primary = getTileset(map->tileset_primary_label);
    map->tileset_secondary = getTileset(map->tileset_secondary_label);
}

Tileset* Project::loadTileset(QString label) {
    Asm *parser = new Asm;

    QString headers_text = readTextFile(root + "/data/tilesets/headers.inc");
    QStringList *values = getLabelValues(parser->parse(headers_text), label);
    Tileset *tileset = new Tileset;
    tileset->name = label;
    tileset->is_compressed = values->value(0);
    tileset->is_secondary = values->value(1);
    tileset->padding = values->value(2);
    tileset->tiles_label = values->value(3);
    tileset->palettes_label = values->value(4);
    tileset->metatiles_label = values->value(5);
    tileset->metatile_attrs_label = values->value(6);
    tileset->callback_label = values->value(7);

    loadTilesetAssets(tileset);

    tileset_cache->insert(label, tileset);
    return tileset;
}

QString Project::getBlockdataPath(Map* map) {
    QString text = readTextFile(root + "/data/maps/_assets.inc");
    QStringList *values = getLabelValues(parse(text), map->blockdata_label);
    QString path;
    if (!values->isEmpty()) {
        path = root + "/" + values->value(0).section('"', 1, 1);
    } else {
        path = root + "/data/maps/" + map->name + "/map.bin";
    }
    return path;
}

QString Project::getMapBorderPath(Map *map) {
    QString text = readTextFile(root + "/data/maps/_assets.inc");
    QStringList *values = getLabelValues(parse(text), map->border_label);
    QString path;
    if (!values->isEmpty()) {
        path = root + "/" + values->value(0).section('"', 1, 1);
    } else {
        path = root + "/data/maps/" + map->name + "/border.bin";
    }
    return path;
}

void Project::loadBlockdata(Map* map) {
    QString path = getBlockdataPath(map);
    map->blockdata = readBlockdata(path);
}

void Project::loadMapBorder(Map *map) {
    QString path = getMapBorderPath(map);
    map->border = readBlockdata(path);
}

void Project::saveBlockdata(Map* map) {
    QString path = getBlockdataPath(map);
    writeBlockdata(path, map->blockdata);
    map->history.save();
}

void Project::writeBlockdata(QString path, Blockdata *blockdata) {
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        QByteArray data = blockdata->serialize();
        file.write(data);
    }
}

void Project::saveAllMaps() {
    QList<QString> keys = map_cache->keys();
    for (int i = 0; i < keys.length(); i++) {
        QString key = keys.value(i);
        Map* map = map_cache->value(key);
        saveMap(map);
    }
}

void Project::saveMap(Map *map) {
    saveBlockdata(map);
    saveMapHeader(map);
    saveMapEvents(map);
}

void Project::loadTilesetAssets(Tileset* tileset) {
    Asm* parser = new Asm;
    QString category = (tileset->is_secondary == "TRUE") ? "secondary" : "primary";
    if (tileset->name.isNull()) {
        return;
    }
    QString dir_path = root + "/data/tilesets/" + category + "/" + tileset->name.replace("gTileset_", "").toLower();

    QString graphics_text = readTextFile(root + "/data/tilesets/graphics.inc");
    QList<QStringList> *graphics = parser->parse(graphics_text);
    QStringList *tiles_values = getLabelValues(graphics, tileset->tiles_label);
    QStringList *palettes_values = getLabelValues(graphics, tileset->palettes_label);

    QString tiles_path;
    if (!tiles_values->isEmpty()) {
        tiles_path = root + "/" + tiles_values->value(0).section('"', 1, 1);
    } else {
        tiles_path = dir_path + "/tiles.4bpp";
        if (tileset->is_compressed == "TRUE") {
            tiles_path += ".lz";
        }
    }

    QStringList *palette_paths = new QStringList;
    if (!palettes_values->isEmpty()) {
        for (int i = 0; i < palettes_values->length(); i++) {
            QString value = palettes_values->value(i);
            palette_paths->append(root + "/" + value.section('"', 1, 1));
        }
    } else {
        QString palettes_dir_path = dir_path + "/palettes";
        for (int i = 0; i < 16; i++) {
            palette_paths->append(palettes_dir_path + "/" + QString("%1").arg(i, 2, 10, QLatin1Char('0')) + ".gbapal");
        }
    }

    QString metatiles_path;
    QString metatile_attrs_path;
    QString metatiles_text = readTextFile(root + "/data/tilesets/metatiles.inc");
    QList<QStringList> *metatiles_macros = parser->parse(metatiles_text);
    QStringList *metatiles_values = getLabelValues(metatiles_macros, tileset->metatiles_label);
    if (!metatiles_values->isEmpty()) {
        metatiles_path = root + "/" + metatiles_values->value(0).section('"', 1, 1);
    } else {
        metatiles_path = dir_path + "/metatiles.bin";
    }
    QStringList *metatile_attrs_values = getLabelValues(metatiles_macros, tileset->metatile_attrs_label);
    if (!metatile_attrs_values->isEmpty()) {
        metatile_attrs_path = root + "/" + metatile_attrs_values->value(0).section('"', 1, 1);
    } else {
        metatile_attrs_path = dir_path + "/metatile_attributes.bin";
    }

    // tiles
    tiles_path = fixGraphicPath(tiles_path);
    QImage *image = new QImage(tiles_path);
    //image->setColor(0, qRgb(0xff, 0, 0)); // debug

    QList<QImage> *tiles = new QList<QImage>;
    int w = 8;
    int h = 8;
    for (int y = 0; y < image->height(); y += h)
    for (int x = 0; x < image->width(); x += w) {
        QImage tile = image->copy(x, y, w, h);
        tiles->append(tile);
    }
    tileset->tiles = tiles;

    // metatiles
    //qDebug() << metatiles_path;
    QFile metatiles_file(metatiles_path);
    if (metatiles_file.open(QIODevice::ReadOnly)) {
        QByteArray data = metatiles_file.readAll();
        int num_metatiles = data.length() / 16;
        int num_layers = 2;
        QList<Metatile*> *metatiles = new QList<Metatile*>;
        for (int i = 0; i < num_metatiles; i++) {
            Metatile *metatile = new Metatile;
            int index = i * (2 * 4 * num_layers);
            for (int j = 0; j < 4 * num_layers; j++) {
                uint16_t word = data[index++] & 0xff;
                word += (data[index++] & 0xff) << 8;
                Tile tile;
                tile.tile = word & 0x3ff;
                tile.xflip = (word >> 10) & 1;
                tile.yflip = (word >> 11) & 1;
                tile.palette = (word >> 12) & 0xf;
                metatile->tiles->append(tile);
            }
            metatiles->append(metatile);
        }
        tileset->metatiles = metatiles;
    } else {
        tileset->metatiles = new QList<Metatile*>;
        qDebug() << QString("Could not open '%1'").arg(metatiles_path);
    }

    QFile attrs_file(metatile_attrs_path);
    //qDebug() << metatile_attrs_path;
    if (attrs_file.open(QIODevice::ReadOnly)) {
        QByteArray data = attrs_file.readAll();
        int num_metatiles = data.length() / 2;
        for (int i = 0; i < num_metatiles; i++) {
            uint16_t word = data[i*2] & 0xff;
            word += (data[i*2 + 1] & 0xff) << 8;
            tileset->metatiles->value(i)->attr = word;
        }
    } else {
        qDebug() << QString("Could not open '%1'").arg(metatile_attrs_path);
    }

    // palettes
    QList<QList<QRgb>> *palettes = new QList<QList<QRgb>>;
    for (int i = 0; i < palette_paths->length(); i++) {
        QString path = palette_paths->value(i);
        // the palettes are not compressed. this should never happen. it's only a precaution.
        path = path.replace(QRegExp("\\.lz$"), "");
        // TODO default to .pal (JASC-PAL)
        // just use .gbapal for now
        QFile file(path);
        QList<QRgb> palette;
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            for (int j = 0; j < 16; j++) {
                uint16_t word = data[j*2] & 0xff;
                word += (data[j*2 + 1] & 0xff) << 8;
                int red = word & 0x1f;
                int green = (word >> 5) & 0x1f;
                int blue = (word >> 10) & 0x1f;
                QRgb color = qRgb(red * 8, green * 8, blue * 8);
                palette.prepend(color);
            }
        } else {
            for (int j = 0; j < 16; j++) {
                palette.append(qRgb(j * 16, j * 16, j * 16));
            }
            qDebug() << QString("Could not open '%1'").arg(path);
        }
        //qDebug() << path;
        palettes->append(palette);
    }
    tileset->palettes = palettes;
}

Blockdata* Project::readBlockdata(QString path) {
    Blockdata *blockdata = new Blockdata;
    //qDebug() << path;
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        for (int i = 0; (i + 1) < data.length(); i += 2) {
            uint16_t word = (data[i] & 0xff) + ((data[i + 1] & 0xff) << 8);
            blockdata->addBlock(word);
        }
    }
    return blockdata;
}

Map* Project::getMap(QString map_name) {
    if (map_cache->contains(map_name)) {
        return map_cache->value(map_name);
    } else {
        Map *map = loadMap(map_name);
        return map;
    }
}

Tileset* Project::getTileset(QString label) {
    if (tileset_cache->contains(label)) {
        return tileset_cache->value(label);
    } else {
        Tileset *tileset = loadTileset(label);
        return tileset;
    }
}

QString Project::readTextFile(QString path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        //QMessageBox::information(0, "Error", QString("Could not open '%1': ").arg(path) + file.errorString());
        qDebug() << QString("Could not open '%1': ").arg(path) + file.errorString();
        return QString();
    }
    QTextStream in(&file);
    QString text = "";
    while (!in.atEnd()) {
        text += in.readLine() + "\n";
    }
    return text;
}

void Project::saveTextFile(QString path, QString text) {
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(text.toUtf8());
    } else {
        qDebug() << QString("Could not open '%1' for writing: ").arg(path) + file.errorString();
    }
}

void Project::readMapGroups() {
    QString text = readTextFile(root + "/data/maps/_groups.inc");
    if (text.isNull()) {
        return;
    }
    Asm *parser = new Asm;
    QList<QStringList> *commands = parser->parse(text);

    bool in_group_pointers = false;
    QStringList *groups = new QStringList;
    for (int i = 0; i < commands->length(); i++) {
        QStringList params = commands->value(i);
        QString macro = params.value(0);
        if (macro == ".label") {
            if (in_group_pointers) {
                break;
            }
            if (params.value(1) == "gMapGroups") {
                in_group_pointers = true;
            }
        } else if (macro == ".4byte") {
            if (in_group_pointers) {
                for (int j = 1; j < params.length(); j++) {
                    groups->append(params.value(j));
                }
            }
        }
    }

    QList<QStringList*> *groupedMaps = new QList<QStringList*>;
    for (int i = 0; i < groups->length(); i++) {
        QStringList *list = new QStringList;
        groupedMaps->append(list);
    }

    QStringList *maps = new QStringList;
    int group = -1;
    for (int i = 0; i < commands->length(); i++) {
        QStringList params = commands->value(i);
        QString macro = params.value(0);
        if (macro == ".label") {
            group = groups->indexOf(params.value(1));
        } else if (macro == ".4byte") {
            if (group != -1) {
                for (int j = 1; j < params.length(); j++) {
                    QStringList *list = groupedMaps->value(group);
                    list->append(params.value(j));
                    maps->append(params.value(j));
                }
            }
        }
    }

    groupNames = groups;
    groupedMapNames = groupedMaps;
    mapNames = maps;
}

QList<QStringList>* Project::parse(QString text) {
    Asm *parser = new Asm;
    return parser->parse(text);
}

QStringList Project::getLocations() {
    // TODO
    QStringList names;
    for (int i = 0; i < 88; i++) {
        names.append(QString("%1").arg(i));
    }
    return names;
}

QStringList Project::getVisibilities() {
    // TODO
    QStringList names;
    for (int i = 0; i < 16; i++) {
        names.append(QString("%1").arg(i));
    }
    return names;
}

QStringList Project::getWeathers() {
    // TODO
    QStringList names;
    for (int i = 0; i < 16; i++) {
        names.append(QString("%1").arg(i));
    }
    return names;
}

QStringList Project::getMapTypes() {
    // TODO
    QStringList names;
    for (int i = 0; i < 16; i++) {
        names.append(QString("%1").arg(i));
    }
    return names;
}

QStringList Project::getBattleScenes() {
    // TODO
    QStringList names;
    for (int i = 0; i < 16; i++) {
        names.append(QString("%1").arg(i));
    }
    return names;
}

QStringList Project::getSongNames() {
    QStringList names;
    QString text = readTextFile(root + "/constants/songs.inc");
    if (!text.isNull()) {
        QList<QStringList> *commands = parse(text);
        for (int i = 0; i < commands->length(); i++) {
            QStringList params = commands->value(i);
            QString macro = params.value(0);
            if (macro == ".equiv") {
                names.append(params.value(1));
            }
        }
    }
    return names;
}

QString Project::getSongName(int value) {
    QStringList names;
    QString text = readTextFile(root + "/constants/songs.inc");
    if (!text.isNull()) {
        QList<QStringList> *commands = parse(text);
        for (int i = 0; i < commands->length(); i++) {
            QStringList params = commands->value(i);
            QString macro = params.value(0);
            if (macro == ".equiv") {
                if (value == ((QString)(params.value(2))).toInt(nullptr, 0)) {
                    return params.value(1);
                }
            }
        }
    }
    return "";
}

QMap<QString, int> Project::getMapObjGfxConstants() {
    QMap<QString, int> constants;
    QString text = readTextFile(root + "/constants/map_object_constants.inc");
    if (!text.isNull()) {
        QList<QStringList> *commands = parse(text);
        for (int i = 0; i < commands->length(); i++) {
            QStringList params = commands->value(i);
            QString macro = params.value(0);
            if (macro == ".set") {
                QString constant = params.value(1);
                if (constant.startsWith("MAP_OBJ_GFX_")) {
                    int value = params.value(2).toInt(nullptr, 0);
                    constants.insert(constant, value);
                }
            }
        }
    }
    return constants;
}

QString Project::fixGraphicPath(QString path) {
    path = path.replace(QRegExp("\\.lz$"), "");
    path = path.replace(QRegExp("\\.[1248]bpp$"), ".png");
    return path;
}

void Project::loadObjectPixmaps(QList<Event*> objects) {
    bool needs_update = false;
    for (Event *object : objects) {
        if (object->pixmap.isNull()) {
            needs_update = true;
            break;
        }
    }
    if (!needs_update) {
        return;
    }

    QMap<QString, int> constants = getMapObjGfxConstants();

    QString pointers_text = readTextFile(root + "/include/data/field_map_obj/map_object_graphics_info_pointers.h");
    QString info_text = readTextFile(root + "/include/data/field_map_obj/map_object_graphics_info.h");
    QString pic_text = readTextFile(root + "/include/data/field_map_obj/map_object_pic_tables.h");
    QString assets_text = readTextFile(root + "/src/field/field_map_obj.c");

    QStringList pointers = readCArray(pointers_text, "gMapObjectGraphicsInfoPointers");

    for (Event *object : objects) {
        if (!object->pixmap.isNull()) {
            continue;
        }
        QString event_type = object->get("event_type");
        if (event_type == "object") {
            object->pixmap = QPixmap(":/images/Entities_16x16.png").copy(0, 0, 16, 16);
        } else if (event_type == "warp") {
            object->pixmap = QPixmap(":/images/Entities_16x16.png").copy(16, 0, 16, 16);
        } else if (event_type == "trap") {
            object->pixmap = QPixmap(":/images/Entities_16x16.png").copy(32, 0, 16, 16);
        } else if (event_type == "sign" || event_type == "hidden item") {
            object->pixmap = QPixmap(":/images/Entities_16x16.png").copy(48, 0, 16, 16);
        }

        if (event_type == "object") {

            int sprite_id = constants.value(object->get("sprite"));

            QString info_label = pointers.value(sprite_id).replace("&", "");
            QString pic_label = readCArray(info_text, info_label).value(14);
            QString gfx_label = readCArray(pic_text, pic_label).value(0);
            gfx_label = gfx_label.section(QRegExp("[\\(\\)]"), 1, 1);
            QString path = readCIncbin(assets_text, gfx_label);

            if (!path.isNull()) {
                path = fixGraphicPath(path);
                QPixmap pixmap(root + "/" + path);
                if (!pixmap.isNull()) {
                    object->pixmap = pixmap;
                }
            }

        }
    }

}

void Project::saveMapEvents(Map *map) {
    QString path = root + QString("/data/maps/events/%1.inc").arg(map->name);
    QString text = "";

    text += QString("%1::\n").arg(map->object_events_label);
    for (int i = 0; i < map->events["object"].length(); i++) {
        Event *object_event = map->events["object"].value(i);
        int radius_x = object_event->getInt("radius_x");
        int radius_y = object_event->getInt("radius_y");
        QString radius = QString("%1").arg((radius_x & 0xf) + ((radius_y & 0xf) << 4));
        uint16_t x = object_event->getInt("x");
        uint16_t y = object_event->getInt("y");

        text += QString("\tobject_event %1").arg(i + 1);
        text += QString(", %1").arg(object_event->get("sprite"));
        text += QString(", %1").arg(object_event->get("replacement"));
        text += QString(", %1").arg(x & 0xff);
        text += QString(", %1").arg((x >> 8) & 0xff);
        text += QString(", %1").arg(y & 0xff);
        text += QString(", %1").arg((y >> 8) & 0xff);
        text += QString(", %1").arg(object_event->get("elevation"));
        text += QString(", %1").arg(object_event->get("behavior"));
        text += QString(", %1").arg(radius);
        text += QString(", 0");
        text += QString(", %1").arg(object_event->get("property"));
        text += QString(", 0");
        text += QString(", %1").arg(object_event->get("sight_radius"));
        text += QString(", 0");
        text += QString(", %1").arg(object_event->get("script_label"));
        text += QString(", %1").arg(object_event->get("event_flag"));
        text += QString(", 0");
        text += QString(", 0");
        text += "\n";
    }
    text += "\n";

    text += QString("%1::\n").arg(map->warps_label);
    for (Event *warp : map->events["warp"]) {
        text += QString("\twarp_def %1").arg(warp->get("x"));
        text += QString(", %1").arg(warp->get("y"));
        text += QString(", %1").arg(warp->get("elevation"));
        text += QString(", %1").arg(warp->get("destination_warp"));
        text += QString(", %1").arg(warp->get("destination_map"));
        text += "\n";
    }
    text += "\n";

    text += QString("%1::\n").arg(map->coord_events_label);
    for (Event *coords : map->events["trap"]) {
        text += QString("\tcoord_event %1").arg(coords->get("x"));
        text += QString(", %1").arg(coords->get("y"));
        text += QString(", %1").arg(coords->get("elevation"));
        text += QString(", 0");
        text += QString(", %1").arg(coords->get("coord_unknown1"));
        text += QString(", %1").arg(coords->get("coord_unknown2"));
        text += QString(", 0");
        text += QString(", %1").arg(coords->get("script_label"));
        text += "\n";
    }
    text += "\n";

    text += QString("%1::\n").arg(map->bg_events_label);
    for (Event *sign : map->events["sign"]) {
        text += QString("\tbg_event %1").arg(sign->get("x"));
        text += QString(", %1").arg(sign->get("y"));
        text += QString(", %1").arg(sign->get("elevation"));
        text += QString(", %1").arg(sign->get("type"));
        text += QString(", 0");
        text += QString(", %1").arg(sign->get("script_label"));
        text += "\n";
    }
    for (Event *item : map->events["hidden item"]) {
        text += QString("\tbg_event %1").arg(item->get("x"));
        text += QString(", %1").arg(item->get("y"));
        text += QString(", %1").arg(item->get("elevation"));
        text += QString(", %1").arg(item->get("type"));
        text += QString(", 0");
        text += QString(", %1").arg(item->get("item"));
        text += QString(", %1").arg(item->get("item_unknown5"));
        text += QString(", %1").arg(item->get("item_unknown6"));
        text += "\n";
    }
    text += "\n";

    text += QString("%1::\n").arg(map->events_label);
    text += QString("\tmap_events %1, %2, %3, %4\n")
            .arg(map->object_events_label)
            .arg(map->warps_label)
            .arg(map->coord_events_label)
            .arg(map->bg_events_label);

    saveTextFile(path, text);
}

void Project::readMapEvents(Map *map) {
    // lazy
    QString path = root + QString("/data/maps/events/%1.inc").arg(map->name);
    QString text = readTextFile(path);
    if (text.isNull()) {
        return;
    }

    QStringList *labels = getLabelValues(parse(text), map->events_label);
    map->object_events_label = labels->value(0);
    map->warps_label = labels->value(1);
    map->coord_events_label = labels->value(2);
    map->bg_events_label = labels->value(3);

    QList<QStringList> *object_events = getLabelMacros(parse(text), map->object_events_label);
    map->events["object"].clear();
    for (QStringList command : *object_events) {
        if (command.value(0) == "object_event") {
            Event *object = new Event;
            object->put("map_name", map->name);
            // This macro is not fixed as of writing, but it should take fewer args.
            bool old_macro = false;
            if (command.length() >= 20) {
                command.removeAt(19);
                command.removeAt(18);
                command.removeAt(15);
                command.removeAt(13);
                command.removeAt(11);
                command.removeAt(1); // id. not 0, but is just the index in the list of objects
                old_macro = true;
            }
            int i = 1;
            object->put("sprite", command.value(i++));
            object->put("replacement", command.value(i++));
            int16_t x = command.value(i++).toInt(nullptr, 0) | (command.value(i++).toInt(nullptr, 0) << 8);
            int16_t y = command.value(i++).toInt(nullptr, 0) | (command.value(i++).toInt(nullptr, 0) << 8);
            object->put("x", x);
            object->put("y", y);
            object->put("elevation", command.value(i++));
            object->put("behavior", command.value(i++));
            if (old_macro) {
                int radius = command.value(i++).toInt(nullptr, 0);
                object->put("radius_x", radius & 0xf);
                object->put("radius_y", (radius >> 4) & 0xf);
            } else {
                object->put("radius_x", command.value(i++));
                object->put("radius_y", command.value(i++));
            }
            object->put("property", command.value(i++));
            object->put("sight_radius", command.value(i++));
            object->put("script_label", command.value(i++));
            object->put("event_flag", command.value(i++));

            object->put("event_type", "object");
            map->events["object"].append(object);
        }
    }

    QList<QStringList> *warps = getLabelMacros(parse(text), map->warps_label);
    map->events["warp"].clear();
    for (QStringList command : *warps) {
        if (command.value(0) == "warp_def") {
            Event *warp = new Event;
            warp->put("map_name", map->name);
            int i = 1;
            warp->put("x", command.value(i++));
            warp->put("y", command.value(i++));
            warp->put("elevation", command.value(i++));
            warp->put("destination_warp", command.value(i++));
            warp->put("destination_map", command.value(i++));

            warp->put("event_type", "warp");
            map->events["warp"].append(warp);
        }
    }

    QList<QStringList> *coords = getLabelMacros(parse(text), map->coord_events_label);
    map->events["trap"].clear();
    for (QStringList command : *coords) {
        if (command.value(0) == "coord_event") {
            Event *coord = new Event;
            coord->put("map_name", map->name);
            bool old_macro = false;
            if (command.length() >= 9) {
                command.removeAt(7);
                command.removeAt(4);
                old_macro = true;
            }
            int i = 1;
            coord->put("x", command.value(i++));
            coord->put("y", command.value(i++));
            coord->put("elevation", command.value(i++));
            coord->put("coord_unknown1", command.value(i++));
            coord->put("coord_unknown2", command.value(i++));
            coord->put("script_label", command.value(i++));
            //coord_unknown3
            //coord_unknown4

            coord->put("event_type", "trap");
            map->events["trap"].append(coord);
        }
    }

    QList<QStringList> *bgs = getLabelMacros(parse(text), map->bg_events_label);
    map->events["hidden item"].clear();
    map->events["sign"].clear();
    for (QStringList command : *bgs) {
        if (command.value(0) == "bg_event") {
            Event *bg = new Event;
            bg->put("map_name", map->name);
            int i = 1;
            bg->put("x", command.value(i++));
            bg->put("y", command.value(i++));
            bg->put("elevation", command.value(i++));
            bg->put("type", command.value(i++));
            i++;
            if (bg->is_hidden_item()) {
                bg->put("item", command.value(i++));
                bg->put("item_unknown5", command.value(i++));
                bg->put("item_unknown6", command.value(i++));

                bg->put("event_type", "hidden item");
                map->events["hidden item"].append(bg);
            } else {
                bg->put("script_label", command.value(i++));
                //sign_unknown7

                bg->put("event_type", "sign");
                map->events["sign"].append(bg);
            }
        }
    }

}

QStringList Project::readCArray(QString text, QString label) {
    QStringList list;

    if (label.isNull()) {
        return list;
    }

    QRegExp *re = new QRegExp(QString("\\b%1\\b\\s*\\[?\\s*\\]?\\s*=\\s*\\{([^\\}]*)\\}").arg(label));
    int pos = re->indexIn(text);
    if (pos != -1) {
        QString body = re->cap(1);
        body = body.replace(QRegExp("\\s*"), "");
        list = body.split(',');
        /*
        QRegExp *inner = new QRegExp("&?\\b([A-Za-z0-9_\\(\\)]*)\\b,");
        int pos = 0;
        while ((pos = inner->indexIn(body, pos)) != -1) {
            list << inner->cap(1);
            pos += inner->matchedLength();
        }
        */
    }

    return list;
}

QString Project::readCIncbin(QString text, QString label) {
    QString path;

    if (label.isNull()) {
        return path;
    }

    QRegExp *re = new QRegExp(QString(
        "\\b%1\\b"
        "\\s*\\[?\\s*\\]?\\s*=\\s*"
        "INCBIN_[US][0-9][0-9]?"
        "\\(\"([^\"]*)\"\\)").arg(label));

    int pos = re->indexIn(text);
    if (pos != -1) {
        path = re->cap(1);
    }

    return path;
}
