import * as React from "react";
import { AFCConfigFile, RatResponse } from "../Lib/RatApiTypes";
import { CardBody, PageSection, Card, CardHead, TextInput, Alert, AlertActionCloseButton } from "@patternfly/react-core";
import DownloadContents from "../Components/DownloadContents";
import { exportCache, putAfcConfigFile, importCache, guiConfig } from "../Lib/RatApi";
import { logger } from "../Lib/Logger";
import { addAuth } from "../Lib/User";

export class Replay extends React.Component{
    
    state = {
        data: "",
        analysisType: "",
        location: "",
        response: ""
    }

    constructor(props) {
        super(props);
    }

    private Replay(){
        try{
            fetch(
                '../ratapi/v1/replay',
                {
                    method:'GET',
                    headers: addAuth({})
                }
            ).then(
                res => {
                    this.setState({ 
                        response : res.headers.get('AnalysisType'),
                        data: res.json(),
                        location: this.state.data['location']
                    });
                }
            );
        }
        catch (e){
            this.setState({response: "No File Found"})
        }
    }

    render() {
        return (
        <PageSection>
            <Card>
                <CardHead>
                    Export
                </CardHead>
                <CardBody>
                    <button onClick={()=>this.Replay()}>Replay</button>
                    <br/>
                    <p>{this.state.response}</p>
                    {console.log(this.state.data)}
                    {console.log(this.state.location)}
                    {console.log(this.state.response)}
                    <br/>
                </CardBody>
            </Card>
        </PageSection>);
    }
}