import React from 'react';
import { CardBody, PageSection, Card, CardHeader, CardTitle } from '@patternfly/react-core';

export class Replay extends React.Component<any, any> {
  state = {
    data: '',
    analysisType: '',
    location: '',
    response: '',
  };

  constructor(props: any) {
    super(props);
  }

  private Replay() {
    try {
      fetch('../ratapi/v1/replay', {
        method: 'GET',
      }).then((res) => {
        this.setState({
          response: res.headers.get('AnalysisType'),
          data: res.json(),
          location: this.state.data['location'],
        });
      });
    } catch (e) {
      this.setState({ response: 'No File Found' });
    }
  }

  render() {
    return (
      <PageSection>
        <Card>
          <CardHeader>
            <CardTitle>Export</CardTitle>
          </CardHeader>
          <CardBody>
            <button onClick={() => this.Replay()}>Replay</button>
            <br />
            <p>{this.state.response}</p>
            <br />
          </CardBody>
        </Card>
      </PageSection>
    );
  }
}

export const ReplayPage = () => <Replay />;
