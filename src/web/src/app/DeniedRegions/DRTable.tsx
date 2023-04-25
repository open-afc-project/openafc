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
    onDelete: (id: number) => void
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

    private apToRow = (ap: AccessPointModel) => ({
        id: ap.id,
        cells: this.props.filterId ? [
            ap.serialNumber,
            ap.certificationId || "",
        ] : [
            ap.serialNumber,
            ap.certificationId || "",
            ap.org,
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
                aria-label="Access Point Table"
                cells={this.columns as any}
                rows={this.props.accessPoints.map(this.apToRow)}
                variant={TableVariant.compact}
                actionResolver={(a, b) => this.actionResolver(a, b)}
            >
                <TableHeader />
                <TableBody />
            </Table>
        );
    }
}
