.pragma library

var DEFAULT_EDGE_MARGIN = 28
var DEFAULT_SPACING = 6

function screenPosition(mapView, coordinate) {
    if (!mapView || !mapView.map || !coordinate || !coordinate.isValid)
        return null
    return mapView.map.fromCoordinate(coordinate, false)
}

function computePlacement(mapView, coordinate, opts) {
    opts = opts || {}
    var margin = opts.edgeMargin !== undefined ? opts.edgeMargin : DEFAULT_EDGE_MARGIN
    var spacing = opts.spacing !== undefined ? opts.spacing : DEFAULT_SPACING
    var labelW = opts.labelWidth || 120
    var labelH = opts.labelHeight || 24
    var iconW = opts.iconWidth || 30
    var iconH = opts.iconHeight || 38
    var pinStem = opts.pinStem || 0
    var preferH = opts.preferHorizontal || "right"
    var preferV = opts.preferVertical || "bottom"
    var layoutStyle = opts.layoutStyle || "row"

    var pt = screenPosition(mapView, coordinate)
    if (!pt)
        return { horizontal: preferH, vertical: preferV }

    var mw = mapView.width
    var mh = mapView.height
    var h = preferH
    var v = preferV

    if (layoutStyle === "labelOnly") {
        h = pt.x + labelW + margin > mw ? "left" : "right"
        v = pt.y + labelH + margin > mh ? "top" : "bottom"
        return { horizontal: h, vertical: v }
    }

    if (layoutStyle === "column") {
        h = "center"
        var totalIconH = iconH + pinStem
        var extendDown = totalIconH / 2 + spacing + labelH + margin
        var extendUp = totalIconH / 2 + spacing + labelH + margin
        if (preferV === "bottom" && pt.y + extendDown > mh)
            v = "top"
        else if (preferV === "top" && pt.y - extendUp < 0)
            v = "bottom"
        else
            v = preferV
    } else {
        var totalIconH = iconH + pinStem
        var extendRight = iconW / 2 + spacing + labelW + margin
        var extendLeft = iconW / 2 + spacing + labelW + margin
        if (pt.x + extendRight > mw)
            h = "left"
        else if (pt.x - extendLeft < 0)
            h = "right"
        else
            h = preferH

        if (pt.y + totalIconH / 2 + margin > mh)
            v = "top"
        else if (pt.y - totalIconH / 2 - margin < 0)
            v = "bottom"
        else
            v = "center"
    }

    return { horizontal: h, vertical: v }
}

function computeAnchor(placement, opts) {
    opts = opts || {}
    var spacing = opts.spacing !== undefined ? opts.spacing : DEFAULT_SPACING
    var labelW = opts.labelWidth || 120
    var labelH = opts.labelHeight || 24
    var iconW = opts.iconWidth || 30
    var iconH = opts.iconHeight || 38
    var pinStem = opts.pinStem || 0
    var layoutStyle = opts.layoutStyle || "row"

    if ((opts.iconWidth || 0) === 0 && (opts.iconHeight || 0) === 0) {
        return {
            anchorX: placement.horizontal === "left" ? labelW : 0,
            anchorY: placement.vertical === "top" ? 0 : labelH,
            contentWidth: labelW,
            contentHeight: labelH
        }
    }

    var totalIconH = iconH + pinStem
    var contentW = iconW
    var contentH = totalIconH
    var anchorX = iconW / 2
    var anchorY = iconH / 2

    if (layoutStyle === "column") {
        contentW = Math.max(iconW, labelW)
        if (placement.vertical === "top") {
            contentH = labelH + spacing + totalIconH
            anchorX = contentW / 2
            anchorY = labelH + spacing + iconH / 2
        } else {
            contentH = totalIconH + spacing + labelH
            anchorX = contentW / 2
            anchorY = iconH / 2
        }
    } else {
        contentH = Math.max(totalIconH, labelH)
        if (placement.horizontal === "left") {
            contentW = labelW + spacing + iconW
            anchorX = labelW + spacing + iconW / 2
        } else {
            contentW = iconW + spacing + labelW
            anchorX = iconW / 2
        }
        anchorY = iconH / 2
    }

    return {
        anchorX: anchorX,
        anchorY: anchorY,
        contentWidth: contentW,
        contentHeight: contentH
    }
}

function metricsForKind(placemarkKind, measured) {
    measured = measured || {}
    if (placemarkKind === "vehicle") {
        return {
            layoutStyle: "column",
            preferHorizontal: "center",
            preferVertical: "bottom",
            iconWidth: 24,
            iconHeight: 24,
            pinStem: 0,
            labelWidth: measured.labelWidth || 90,
            labelHeight: measured.labelHeight || 20
        }
    }
    if (placemarkKind === "target") {
        return {
            layoutStyle: "row",
            preferHorizontal: "right",
            preferVertical: "center",
            iconWidth: 30,
            iconHeight: 30,
            pinStem: 8,
            labelWidth: measured.labelWidth || 160,
            labelHeight: measured.labelHeight || 28
        }
    }
    if (placemarkKind === "geoName") {
        return {
            layoutStyle: "labelOnly",
            preferHorizontal: "right",
            preferVertical: "bottom",
            iconWidth: 0,
            iconHeight: 0,
            pinStem: 0,
            labelWidth: measured.labelWidth || 160,
            labelHeight: measured.labelHeight || 28
        }
    }
    return {
        layoutStyle: "row",
        preferHorizontal: "right",
        preferVertical: "bottom",
        iconWidth: measured.iconWidth || 30,
        iconHeight: measured.iconHeight || 30,
        pinStem: measured.pinStem || 0,
        labelWidth: measured.labelWidth || 120,
        labelHeight: measured.labelHeight || 24
    }
}
