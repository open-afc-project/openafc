import React from 'react';
import { UserModel } from '../Lib/RatApiTypes';
import { Table, Thead, Tr, Th, Tbody, Td, ActionsColumn } from '@patternfly/react-table';

/**
 * UserList.tsx: Table of users with actions
 * author: Sam Smucny
 */

/**
 * mock data
 */
export const testUsers: UserModel[] = [
  // @ts-ignore
  {
    id: 1,
    email: 'a@domain.com',
    firstName: 'Bob',
    active: true,
    roles: ['Analysis', 'AP'],
  },
];

interface UserTableProps {
  onDelete: (id: number) => void;
  onRoleAdd: (id: number) => void;
  onRoleRemove: (id: number) => void;
  onUserEdit: (id: number) => void;
  users: UserModel[];
}

/**
 * Table component to show users
 */
export class UserTable extends React.Component<UserTableProps, {}> {
  private columns = [{ title: 'Email' }, { title: 'Org' }, { title: 'Active' }, { title: 'Roles' }];

  constructor(props: UserTableProps) {
    super(props);
    this.state = {
      rows: [],
    };
  }

  render() {
    return (
      <Table aria-label="User Table" variant="compact">
        <Thead>
          <Tr>
            {this.columns.map((col, idx) => (
              <Th key={idx}>{col.title}</Th>
            ))}
            <Th screenReaderText="Actions" />
          </Tr>
        </Thead>
        <Tbody>
          {this.props.users.map((u, index) => (
            <Tr key={u.id}>
              <Td dataLabel={this.columns[0].title}>{u.email}</Td>
              <Td dataLabel={this.columns[1].title}>{u.org}</Td>
              <Td dataLabel={this.columns[2].title}>{u.active ? 'Y' : 'N'}</Td>
              <Td dataLabel={this.columns[3].title}>{u.roles.join(', ')}</Td>
              <Td isActionCell>
                <ActionsColumn
                  items={[
                    {
                      title: 'Edit User',
                      onClick: () => this.props.onUserEdit(u.id),
                    },
                    {
                      title: 'Add Role',
                      onClick: () => this.props.onRoleAdd(u.id),
                    },
                    {
                      title: 'Remove Role',
                      onClick: () => this.props.onRoleRemove(u.id),
                    },
                    {
                      title: 'Delete',
                      onClick: () => this.props.onDelete(u.id),
                    },
                  ]}
                />
              </Td>
            </Tr>
          ))}
        </Tbody>
      </Table>
    );
  }
}
