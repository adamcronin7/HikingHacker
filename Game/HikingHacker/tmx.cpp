#include "tmx.h"

#include <tinyxml2.h>
#include <filesystem>


using namespace tmx;

std::unique_ptr<Map> tmx::loadMap(const std::string& mapFilePath)
{
	// load TMX map
	using namespace tinyxml2;
	std::filesystem::path mapPath(mapFilePath);

	tmx::Map* map = nullptr;

	XMLDocument doc;
	doc.LoadFile(mapPath.string().c_str());
	XMLElement* mapDoc = doc.FirstChildElement("map");
	if (mapDoc)
	{
		map = new tmx::Map;
		map->mapWidth = mapDoc->IntAttribute("width");
		map->mapHeight = mapDoc->IntAttribute("height");
		map->tileWidth = mapDoc->IntAttribute("tilewidth");
		map->tileHeight = mapDoc->IntAttribute("tileheight");

		for (XMLElement* child = mapDoc->FirstChildElement();
			child != nullptr; child = child->NextSiblingElement())
		{
			if (strcmp(child->Name(), "tileset") == 0)
			{
				int firstgid = child->IntAttribute("firstgid");

				// load tileset XML
				XMLDocument tilesetDoc;
				auto sourcePath = mapPath.parent_path().append(child->Attribute("source"));
				tilesetDoc.LoadFile(sourcePath.string().c_str());

				XMLElement* ts = tilesetDoc.FirstChildElement("tileset");
				if (ts)
				{
					int tileWidth = ts->IntAttribute("tilewidth");
					int tileHeight = ts->IntAttribute("tileheight");
					int count = ts->IntAttribute("tilecount");
					int columns = ts->IntAttribute("columns");
					tmx::TileSet newTileSet(firstgid, count, tileWidth, tileHeight, columns);


					newTileSet.spacing = ts->IntAttribute("spacing");
					newTileSet.margin = ts->IntAttribute("margin");
					if (XMLElement* img = ts->FirstChildElement("image"))
					{
						newTileSet.image.source = img->Attribute("source");
						newTileSet.image.width = img->IntAttribute("width");
						newTileSet.image.height = img->IntAttribute("height");
					}


					// process individual tile elements
					for (XMLElement* tile = ts->FirstChildElement("tile");
						tile != nullptr;
						tile = tile->NextSiblingElement("tile"))
					{
						Tile newTile;
						newTile.id = tile->IntAttribute("id");


						// images / sources
						XMLElement* image = tile->FirstChildElement("image");
						if (image)
						{
							newTile.image.source = image->Attribute("source");
							newTile.image.width = image->IntAttribute("width");
							newTile.image.height = image->IntAttribute("height");
						}

						newTileSet.tiles.push_back(newTile);
					}

					map->tileSets.push_back(std::move(newTileSet));
				}
			}
			else if (strcmp(child->Name(), "layer") == 0)
			{
				tmx::Layer layer;
				layer.name = child->Attribute("name");
				layer.id = child->IntAttribute("id");
				if (XMLElement* properties = child->FirstChildElement("properties"))
				{
					for (XMLElement* property = properties->FirstChildElement("property");
						property != nullptr; property = property->NextSiblingElement("property"))
					{
						const char* propertyName = property->Attribute("name");
						const char* propertyValue = property->Attribute("value");
						if (propertyName && std::strcmp(propertyName, "collidable") == 0)
						{
							layer.collidable = propertyValue
								&& (std::strcmp(propertyValue, "true") == 0 || std::strcmp(propertyValue, "1") == 0);
						}
					}
				}
				layer.data.reserve(map->mapWidth * map->mapHeight);

				XMLElement* data = child->FirstChildElement("data");
				std::stringstream dataStream(data->GetText());
				for (uint32_t i; dataStream >> i;)
				{
					layer.data.push_back(i);
					if (dataStream.peek() == ',') dataStream.ignore();
				}
				map->layers.push_back(std::move(layer));
			}
			else if (strcmp(child->Name(), "objectgroup") == 0)
			{
				tmx::ObjectGroup layer;
				layer.name = child->Attribute("name");
				layer.id = child->IntAttribute("id");

				for (XMLElement* elem = child->FirstChildElement("object");
					elem != nullptr; elem = elem->NextSiblingElement())
				{
					tmx::LayerObject obj;
					obj.id = elem->IntAttribute("id");
					obj.x = elem->FloatAttribute("x");
					obj.y = elem->FloatAttribute("y");

					const char* attrName = elem->Attribute("name");
					if (attrName)
					{
						obj.name = attrName;
					}

					const char* attrType = elem->Attribute("type");
					if (attrType)
					{
						obj.type = attrType;
					}
					layer.objects.push_back(obj);
				}
				map->layers.push_back(std::move(layer));
			}
		}
	}
	return std::unique_ptr<tmx::Map>(map);
}