"""Tests of r3.colors.out"""

import grass.script as gs


def validate_plain_text_output(data):
    """Validate the structure and content of the plain text output."""
    assert data
    assert len(data) == 8, "The output does not match the expected number of items (8)."
    expected = {
        "1 0:191:191",
        "1.8 0:255:0",
        "2.6 255:255:0",
        "3.4 255:127:0",
        "4.2 191:127:63",
        "5 200:200:200",
        "nv 255:255:255",
        "default 255:255:255",
    }
    assert expected == data.keys(), (
        f"test failed: expected {expected} but got {data.keys()}"
    )


def test_r3_colors_out_plain_output(raster3_color_dataset):
    """Test r3.colors.out command for plain output format."""
    session = raster3_color_dataset
    data = gs.parse_command("r3.colors.out", map="b", format="plain", env=session.env)
    validate_plain_text_output(data)


def test_r3_colors_out_without_format_option(raster3_color_dataset):
    """Test r3.colors.out command without any format option."""
    session = raster3_color_dataset
    data = gs.parse_command("r3.colors.out", map="b", env=session.env)
    validate_plain_text_output(data)


def test_r3_colors_out_with_p_flag(raster3_color_dataset):
    """Test r3.colors.out command with percentage values."""
    session = raster3_color_dataset
    data = gs.parse_command("r3.colors.out", map="b", flags="p", env=session.env)
    assert data
    assert len(data) == 8, "The output does not match the expected number of items (8)."
    expected = {
        "0% 0:191:191",
        "20% 0:255:0",
        "40% 255:255:0",
        "60% 255:127:0",
        "80% 191:127:63",
        "100% 200:200:200",
        "nv 255:255:255",
        "default 255:255:255",
    }
    assert expected == data.keys(), (
        f"test failed: expected {expected} but got {data.keys()}"
    )


def validate_common_json_structure(data):
    """Validate the common structure and content of the JSON output."""
    assert isinstance(data, list), "Output data should be a list of entries."
    assert len(data) == 8, (
        "The length of the output JSON does not match the expected value of 8."
    )


def test_r3_colors_out_json_with_default_option(raster3_color_dataset):
    """Test r3.colors.out command for JSON output format for default color option."""
    session = raster3_color_dataset
    data = gs.parse_command("r3.colors.out", map="b", format="json", env=session.env)
    validate_common_json_structure(data)
    expected = [
        {"value": 1, "color": "#00BFBF"},
        {"value": 1.8, "color": "#00FF00"},
        {"value": 2.6, "color": "#FFFF00"},
        {"value": 3.4, "color": "#FF7F00"},
        {"value": 4.2, "color": "#BF7F3F"},
        {"value": 5, "color": "#C8C8C8"},
        {"value": "nv", "color": "#FFFFFF"},
        {"value": "default", "color": "#FFFFFF"},
    ]
    assert expected == data, f"test failed: expected {expected} but got {data}"


def test_r3_colors_out_json_with_triplet_option(raster3_color_dataset):
    """Test r3.colors.out command for JSON output format for triplet color option."""
    session = raster3_color_dataset
    data = gs.parse_command(
        "r3.colors.out", map="b", format="json", color_format="triplet", env=session.env
    )
    validate_common_json_structure(data)
    expected = [
        {"value": 1, "color": "0:191:191"},
        {"value": 1.8, "color": "0:255:0"},
        {"value": 2.6, "color": "255:255:0"},
        {"value": 3.4, "color": "255:127:0"},
        {"value": 4.2, "color": "191:127:63"},
        {"value": 5, "color": "200:200:200"},
        {"value": "nv", "color": "255:255:255"},
        {"value": "default", "color": "255:255:255"},
    ]
    assert expected == data, f"test failed: expected {expected} but got {data}"


def test_r3_colors_out_json_with_rgb_option(raster3_color_dataset):
    """Test r3.colors.out command for JSON output format for rgb color option."""
    session = raster3_color_dataset
    data = gs.parse_command(
        "r3.colors.out",
        map="b",
        format="json",
        color_format="rgb",
        env=session.env,
    )
    validate_common_json_structure(data)
    expected = [
        {"value": 1, "color": "rgb(0, 191, 191)"},
        {"value": 1.8, "color": "rgb(0, 255, 0)"},
        {"value": 2.6, "color": "rgb(255, 255, 0)"},
        {"value": 3.4, "color": "rgb(255, 127, 0)"},
        {"value": 4.2, "color": "rgb(191, 127, 63)"},
        {"value": 5, "color": "rgb(200, 200, 200)"},
        {"value": "nv", "color": "rgb(255, 255, 255)"},
        {"value": "default", "color": "rgb(255, 255, 255)"},
    ]
    assert expected == data, f"test failed: expected {expected} but got {data}"


def test_r3_colors_out_json_with_hex_option(raster3_color_dataset):
    """Test r3.colors.out command for JSON output format for hex color option."""
    session = raster3_color_dataset
    data = gs.parse_command(
        "r3.colors.out",
        map="b",
        format="json",
        color_format="hex",
        env=session.env,
    )
    validate_common_json_structure(data)
    expected = [
        {"value": 1, "color": "#00BFBF"},
        {"value": 1.8, "color": "#00FF00"},
        {"value": 2.6, "color": "#FFFF00"},
        {"value": 3.4, "color": "#FF7F00"},
        {"value": 4.2, "color": "#BF7F3F"},
        {"value": 5, "color": "#C8C8C8"},
        {"value": "nv", "color": "#FFFFFF"},
        {"value": "default", "color": "#FFFFFF"},
    ]
    assert expected == data, f"test failed: expected {expected} but got {data}"


def test_r3_colors_out_json_with_hsv_option(raster3_color_dataset):
    """Test r3.colors.out command for JSON output format for hsv color option."""
    session = raster3_color_dataset
    data = gs.parse_command(
        "r3.colors.out",
        map="b",
        format="json",
        color_format="hsv",
        env=session.env,
    )
    validate_common_json_structure(data)
    expected = [
        {"value": 1, "color": "hsv(180, 100, 74)"},
        {"value": 1.8, "color": "hsv(120, 100, 100)"},
        {"value": 2.6, "color": "hsv(60, 100, 100)"},
        {"value": 3.4, "color": "hsv(29, 100, 100)"},
        {"value": 4.2, "color": "hsv(30, 67, 74)"},
        {"value": 5, "color": "hsv(0, 0, 78)"},
        {"value": "nv", "color": "hsv(0, 0, 100)"},
        {"value": "default", "color": "hsv(0, 0, 100)"},
    ]
    assert expected == data, f"test failed: expected {expected} but got {data}"
