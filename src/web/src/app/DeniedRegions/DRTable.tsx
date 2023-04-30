import * as React from "react";
import { headerCol, Table, TableVariant, TableHeader, TableBody } from "@patternfly/react-table";
import { AccessPointModel, DeniedRegion, ExclusionCircle, ExclusionHorizon, ExclusionRect, ExclusionTwoRect, UserModel } from "../Lib/RatApiTypes";

/**
 * DRTable.tsx: Table that displays access points. Shows org column if admin specifies filterId is 0
 * author: Sam Smucny
 */

/**
 * Interface definition of `DRTable` properties
 */
interface DRTableProps {
    deniedRegions: DeniedRegion[],
    currentRegionStr: string,
    onDelete: (id: string) => void
}

/**
 * Table component to display access points.
 */
export class DRTable extends React.Component<DRTableProps, {}> {

    private columns = [
        "Location", "Start Freq (MHz)", "Stop Freq (MHz)", "Exclusion Zone", "Coordinates"
    ]

    constructor(props: DRTableProps) {
        super(props);
        this.state = {
            rows: []
        };

    }

    private toRow = (dr: DeniedRegion) => ({
        id: dr.name + dr.zoneType,
        cells: [
            dr.name, dr.startFreq, dr.endFreq, dr.zoneType, this.zoneToText(dr)
        ]
    })

    private zoneToText(dr: DeniedRegion) {
        switch (dr.zoneType) {
            case "Circle":
                let c = dr.exclusionZone as ExclusionCircle;
                return `Center: (${c.latitude}, ${c.longitude}) Rad: ${c.radiusKm} km`
            case "One Rectangle":
                let o = dr.exclusionZone as ExclusionRect;
                return `(${o.topLat}, ${o.leftLong}), (${o.bottomLat}, ${o.rightLong}) `
            case "Two Rectangles":
                let t = dr.exclusionZone as ExclusionTwoRect;
                return `Rectangle 1: (${t.rectangleOne.topLat}, ${t.rectangleOne.leftLong}), (${t.rectangleOne.bottomLat}, ${t.rectangleOne.rightLong}) ` +
                    `Rectangle 2:(${t.rectangleTwo.topLat}, ${t.rectangleTwo.leftLong}), (${t.rectangleTwo.bottomLat}, ${t.rectangleTwo.rightLong})`;
            case "Horizon Distance":
                let h = dr.exclusionZone as ExclusionHorizon;
                return `Center: (${h.latitude}, ${h.longitude}) Height AGL: ${h.aglHeightM} m`
            default:
                return "";
        }
    }

    actionResolver(data: any, extraData: any) {
        return [
            {
                title: "Remove",
                onClick: (event: any, rowId: number, rowData: any, extra: any) => this.props.onDelete(rowData.id)
            }
        ];
    }

    render() {
        return (
            <Table
                aria-label="Denied Region Table"
                cells={this.columns as any}
                rows={this.props.deniedRegions.filter(x => x.regionStr == this.props.currentRegionStr).map(this.toRow)}
                variant={TableVariant.compact}
                actionResolver={(a, b) => this.actionResolver(a, b)}
            >
                <TableHeader />
                <TableBody />
            </Table>
        );
    }
}
