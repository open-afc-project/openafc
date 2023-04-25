import * as React from "react";
import { headerCol, Table, TableVariant, TableHeader, TableBody } from "@patternfly/react-table";
import { AccessPointModel, DeniedRegion, UserModel } from "../Lib/RatApiTypes";

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
        "Location","Start Freq (MHz)","Stop Freq (MHz)","Exclusion Zone"
    ]

    constructor(props: DRTableProps) {
        super(props);
        this.state = {
            rows: []
        };

    }

    private toRow = (dr: DeniedRegion) => ({
        id: dr.name+dr.zoneType,
        cells: [
            dr.name, dr.startFreq, dr.endFreq, dr.zoneType
        ]
    })

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
                rows={this.props.deniedRegions.filter(x=>x.regionStr == this.props.currentRegionStr).map(this.toRow)}
                variant={TableVariant.compact}
                actionResolver={(a, b) => this.actionResolver(a, b)}
            >
                <TableHeader />
                <TableBody />
            </Table>
        );
    }
}
