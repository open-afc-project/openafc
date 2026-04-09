import React from 'react';
import { Table, Thead, Tr, Th, Tbody, Td, ActionsColumn } from '@patternfly/react-table';
import {
  AccessPointModel,
  DeniedRegion,
  ExclusionCircle,
  ExclusionHorizon,
  ExclusionRect,
  ExclusionTwoRect,
  UserModel,
} from '../Lib/RatApiTypes';

/**
 * DRTable.tsx: Table that displays access points. Shows org column if admin specifies filterId is 0
 * author: Sam Smucny
 */

/**
 * Interface definition of `DRTable` properties
 */
interface DRTableProps {
  deniedRegions: DeniedRegion[];
  currentRegionStr: string;
  onDelete: (id: string) => void;
  onOpenEdit: (id: string) => void;
}

/**
 * Table component to display access points.
 */
export class DRTable extends React.Component<DRTableProps, {}> {
  private columns = ['Location', 'Start Freq (MHz)', 'Stop Freq (MHz)', 'Exclusion Zone', 'Coordinates'];

  constructor(props: DRTableProps) {
    super(props);
    this.state = {
      rows: [],
    };
  }

  private zoneToText(dr: DeniedRegion) {
    switch (dr.zoneType) {
      case 'Circle':
        const c = dr.exclusionZone as ExclusionCircle;
        return `Center: (${c.latitude}, ${c.longitude}) Rad: ${c.radiusKm} km`;
      case 'One Rectangle':
        const o = dr.exclusionZone as ExclusionRect;
        return `(${o.topLat}, ${o.leftLong}), (${o.bottomLat}, ${o.rightLong}) `;
      case 'Two Rectangles':
        const t = dr.exclusionZone as ExclusionTwoRect;
        return (
          `Rectangle 1: (${t.rectangleOne.topLat}, ${t.rectangleOne.leftLong}), (${t.rectangleOne.bottomLat}, ${t.rectangleOne.rightLong}) ` +
          `Rectangle 2:(${t.rectangleTwo.topLat}, ${t.rectangleTwo.leftLong}), (${t.rectangleTwo.bottomLat}, ${t.rectangleTwo.rightLong})`
        );
      case 'Horizon Distance':
        const h = dr.exclusionZone as ExclusionHorizon;
        return `Center: (${h.latitude}, ${h.longitude}) Height AGL: ${h.aglHeightM} m`;
      default:
        return '';
    }
  }

  render() {
    const filteredRegions = Array.isArray(this.props.deniedRegions)
      ? this.props.deniedRegions.filter((x) => x.regionStr == this.props.currentRegionStr)
      : [];

    return (
      <Table aria-label="Denied Region Table" variant="compact">
        <Thead>
          <Tr>
            {this.columns.map((col, idx) => (
              <Th key={idx}>{col}</Th>
            ))}
            <Th screenReaderText="Actions" />
          </Tr>
        </Thead>
        <Tbody>
          {filteredRegions.map((dr, index) => {
            const id = dr.name + '===' + dr.zoneType;
            return (
              <Tr key={id}>
                <Td dataLabel={this.columns[0]}>{dr.name}</Td>
                <Td dataLabel={this.columns[1]}>{dr.startFreq}</Td>
                <Td dataLabel={this.columns[2]}>{dr.endFreq}</Td>
                <Td dataLabel={this.columns[3]}>{dr.zoneType}</Td>
                <Td dataLabel={this.columns[4]}>{this.zoneToText(dr)}</Td>
                <Td isActionCell>
                  <ActionsColumn
                    items={[
                      {
                        title: 'Edit',
                        onClick: () => this.props.onOpenEdit(id),
                      },
                      {
                        title: 'Remove',
                        onClick: () => this.props.onDelete(id),
                      },
                    ]}
                  />
                </Td>
              </Tr>
            );
          })}
        </Tbody>
      </Table>
    );
  }
}
