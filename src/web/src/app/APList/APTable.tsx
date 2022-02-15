import * as React from "react";
import { headerCol, Table, TableVariant, TableHeader, TableBody } from "@patternfly/react-table";
import { AccessPointModel, UserModel } from "../Lib/RatApiTypes";

/**
 * APTable.tsx: Table that displays access points. Shows owner column if admin specifies user list
 * author: Sam Smucny
 */

/**
 * Interface definition of `APTable` properties
 */
interface APTableProps {
    accessPoints: AccessPointModel[],
    /**
     * If `users` is non-empty then the owner column will be displayed (Admin feature)
     */
    users?: UserModel[],
    onDelete: (id: number) => void
}

/**
 * Table component to display a user's access points.
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

        if (props.users) {
            this.columns.push({
                title: "Owner"
            });
        }
    }

    private apToRow = (ap: AccessPointModel) => ({
        id: ap.id,
        cells: this.props.users ? [
            ap.serialNumber,
            ap.certificationId || "",
           this.props.users.find(x => x.id === ap.ownerId)?.email || "NO USER"
        ] : [
            ap.serialNumber,
            ap.certificationId || "",
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