import * as React from "react";
import { headerCol, Table, TableVariant, TableHeader, TableBody } from "@patternfly/react-table";
import { AccessPointModel, UserModel } from "../Lib/RatApiTypes";

/**
 * APTable.tsx: Table that displays access points. Shows org column if admin specifies filterId is 0
 * author: Sam Smucny
 */

/**
 * Interface definition of `APTable` properties
 */
interface APTableProps {
    accessPoints: AccessPointModel[],
    /**
     * If `filterId` is 0 then the org column will be displayed (Super admin feature)
     */
    filterId?: number,
    onDelete: (id: number) => void
}

/**
 * Table component to display access points.
 */
export class APTable extends React.Component<APTableProps, {}> {

    private columns = [
        { title: "Serial Number", cellTransforms: [headerCol()] },
        { title: "Certification ID" },
    ]

    constructor(props: APTableProps) {
        super(props);
        this.state = {
            rows: []
        };

        if (props.filterId === 0) {
            this.columns.push({
                title: "Org"
            });
        }
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
