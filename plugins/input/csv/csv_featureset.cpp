/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2015 Artem Pavlenko
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

// mapnik
#include "csv_featureset.hpp"
#include <mapnik/debug.hpp>
#include <mapnik/feature.hpp>
#include <mapnik/feature_factory.hpp>
#include <mapnik/util/utf_conv_win.hpp>
// stl
#include <string>
#include <vector>
#include <deque>

csv_featureset::csv_featureset(std::string const& filename, detail::geometry_column_locator const& locator, std::string const& separator,
                               std::vector<std::string> const& headers, mapnik::context_ptr const& ctx, array_type && index_array)
    :
#ifdef _WINDOWS
    file_(_wfopen(mapnik::utf8_to_utf16(filename).c_str(), L"rb"), std::fclose),
#else
    file_(std::fopen(filename.c_str(),"rb"), std::fclose),
#endif
    separator_(separator),
    headers_(headers),
    index_array_(std::move(index_array)),
    index_itr_(index_array_.begin()),
    index_end_(index_array_.end()),
    ctx_(ctx),
    locator_(locator),
    tr_("utf8")
{
    if (!file_) throw std::runtime_error("Can't open " + filename);
}

csv_featureset::~csv_featureset() {}

mapnik::feature_ptr csv_featureset::parse_feature(std::string const& str)
{
    auto values = csv_utils::parse_line(str, separator_);
    auto geom = detail::extract_geometry(values, locator_);
    if (!geom.is<mapnik::geometry::geometry_empty>())
    {
        mapnik::feature_ptr feature(mapnik::feature_factory::create(ctx_, ++feature_id_));
        feature->set_geometry(std::move(geom));
        detail::process_properties(*feature, headers_, values, locator_, tr_);
        return feature;
    }
    return mapnik::feature_ptr();
}

mapnik::feature_ptr csv_featureset::next()
{
    if (index_itr_ != index_end_)
    {
        csv_datasource::item_type const& item = *index_itr_++;
        std::size_t file_offset = item.second.first;
        std::size_t size = item.second.second;
        std::fseek(file_.get(), file_offset, SEEK_SET);
        std::vector<char> record;
        record.resize(size);
        std::fread(record.data(), size, 1, file_.get());
        auto const* start = record.data();
        auto const*  end = start + record.size();
        std::string str(start, end);
        return parse_feature(str);
    }
    return mapnik::feature_ptr();
}