import * as React from "react";
import { headerCol, Table, TableVariant, TableHeader, TableBody } from "@patternfly/react-table";
import { AccessPointModel, UserModel } from "../Lib/RatApiTypes";

/**
 * MTLS.tsx: Table that displays mtls certs. Shows owner column if admin specifies user list
 * author: Sam Smucny
 */

/**
 * Interface definition of `MTLSTable` properties
 */
interface MTLSTableProps {
    mtlsList: MTLSModel[],
    /**
     * If `filterId` is 0 then the org column will be displayed (Super admin feature)
     */
    filterId: number,
    onDelete: (id: number) => void
}

/**
 * Table component to display a user's access points.
 */
export class MTLSTable extends React.Component<MTLSTableProps, {}> {

    private columns = [
        { title: "Certificate ID" , cellTransforms: [headerCol()] },
        { title: "Note" },
        { title: "Created" },
       
    ]

    constructor(props: MTLSTableProps) {
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

    private mtlsToRow = (mtls: MTLSModel) => ({
        id: mtls.id,
        cells: this.props.filterId ?  [
            mtls.id,
            mtls.note || "",
            mtls.created || "",
        ] :
        [
            mtls.id,
            mtls.note || "",
            mtls.created || "",
            mtls.org,
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
                aria-label="MTLS Table"
                cells={this.columns as any}
                rows={this.props.mtlsList.map(this.mtlsToRow)}
                variant={TableVariant.compact}
                actionResolver={(a, b) => this.actionResolver(a, b)}
            >
                <TableHeader />
                <TableBody />
            </Table>
        );
    }
}
