import React from 'react';
import { Table, Thead, Tr, Th, Tbody, Td, ActionsColumn } from '@patternfly/react-table';
import { AccessPointModel, MTLSModel, UserModel } from '../Lib/RatApiTypes';

/**
 * MTLS.tsx: Table that displays mtls certs. Shows org column if filterId is 0
 * author: Huy Ton
 */

/**
 * Interface definition of `MTLSTable` properties
 */
interface MTLSTableProps {
  mtlsList: MTLSModel[];
  /**
   * If `filterId` is 0 then the org column will be displayed (Super admin feature)
   */
  filterId: number;
  onDelete: (id: number) => void;
}

/**
 * Table component to display a user's access points.
 */
export class MTLSTable extends React.Component<MTLSTableProps, {}> {
  private columns = [{ title: 'Certificate ID' }, { title: 'Note' }, { title: 'Created' }];

  constructor(props: MTLSTableProps) {
    super(props);
    this.state = {
      rows: [],
    };

    if (props.filterId === 0) {
      this.columns.push({
        title: 'Org',
      });
    }
  }

  render() {
    return (
      <Table aria-label="MTLS Table" variant="compact">
        <Thead>
          <Tr>
            {this.columns.map((col, idx) => (
              <Th key={idx}>{col.title}</Th>
            ))}
            <Th screenReaderText="Actions" />
          </Tr>
        </Thead>
        <Tbody>
          {this.props.mtlsList.map((mtls, index) => (
            <Tr key={mtls.id}>
              <Td dataLabel={this.columns[0].title}>{mtls.id}</Td>
              <Td dataLabel={this.columns[1].title}>{mtls.note || ''}</Td>
              <Td dataLabel={this.columns[2].title}>{mtls.created || ''}</Td>
              {this.props.filterId === 0 && <Td dataLabel={this.columns[3].title}>{mtls.org}</Td>}
              <Td isActionCell>
                <ActionsColumn
                  items={[
                    {
                      title: 'Remove',
                      onClick: () => this.props.onDelete(mtls.id),
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
