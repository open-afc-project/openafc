import React from "react";
import { UserModel } from "../Lib/RatApiTypes";
import {
    Table,
    TableHeader,
    TableBody,
    headerCol,
    TableVariant,
} from "@patternfly/react-table";

/**
 * UserList.tsx: Table of users with actions
 * author: Sam Smucny
 */

/**
 * mock data
 */
export const testUsers: UserModel[] = [
    {
        id: 1,
        email: "a@domain.com",
        firstName: "Bob",
        active: true,
        roles: ["Analysis", "AP"]
    }
]

interface UserTableProps {
    onDelete: (id: number) => void,
    onRoleAdd: (id: number) => void,
    onRoleRemove: (id: number) => void,
    onAPEdit: (id: number) => void,
    onUserEdit: (id: number) => void,
    users: UserModel[]
}

const userToRow = (u: UserModel) => ({
    id: u.id,
    cells: [ u.email, u.active ? "Y" : "N", u.roles.join(", ")]
})

/**
 * Table component to show users
 */
export class UserTable extends React.Component<UserTableProps, {}> {

    private columns = [
        { title: "Email", cellTransforms: [headerCol()] },
        { title: "Active" },
        { title: "Roles" }
    ]

    constructor(props: UserTableProps) {
        super(props);
        this.state = {
            rows: []
        };
    }

    actionResolver(data: any, extraData: any) {

        return [
            {
                title: "Edit APs",
                onClick: (event: any, rowId: number, rowData: any, extra: any) => this.props.onAPEdit(rowData.id)
            },
            {
                title: "Edit User",
                onClick: (event: any, rowId: number, rowData: any, extra: any) => this.props.onUserEdit(rowData.id)
            },
            {
                title: "Add Role",
                onClick: (event: any, rowId: number, rowData: any, extra: any) => this.props.onRoleAdd(rowData.id)
            },
            {
                title: "Remove Role",
                onClick: (event: any, rowId: number, rowData: any, extra: any) => this.props.onRoleRemove(rowData.id)
            },
            {
                title: "Delete",
                onClick: (event: any, rowId: any, rowData: any, extra: any) => this.props.onDelete(rowData.id)
            }
        ];
    }

    render() {
        return (
            <Table
                aria-label="User Table"
                cells={this.columns as any}
                rows={this.props.users.map(userToRow)}
                actionResolver={(a, b) => this.actionResolver(a, b)}
                // areActionsDisabled={this.areActionsDisabled}
                variant={TableVariant.compact}
            >
                <TableHeader />
                <TableBody />
            </Table>
        );
    }
}