import React from "react";
import { AboutModal, Button, TextContent, TextList, TextListItem } from "@patternfly/react-core";
import { guiConfig } from "../Lib/RatApi";
import { InfoIcon } from "@patternfly/react-icons";

/**
 * AppInfo.tsx: Application about modal with version information
 * author: Sam Smucny
 */

/**
 * Modal component that provides information about the application
 */
class AppInfo extends React.Component<{}, { isModalOpen: boolean }> {
    handleModalToggle: () => void;

    constructor(props: any) {
        super(props);
        this.state = {
            isModalOpen: false
        };
        this.handleModalToggle = () => {
            this.setState(({ isModalOpen }) => ({
                isModalOpen: !isModalOpen
            }));
        };
    }

    render() {
        const { isModalOpen } = this.state;

        return (
            <React.Fragment>
                <Button variant="plain" isInline={true} onClick={this.handleModalToggle}>
                    <InfoIcon/>
                </Button>
                <AboutModal
                    isOpen={isModalOpen}
                    onClose={this.handleModalToggle}
                    trademark=""
                    brandImageSrc=""
                    brandImageAlt="Logo"
                    productName="RLAN AFC Tool"
                >
                    <TextContent>
                        <TextList component="dl">
                            <TextListItem component="dt">Version</TextListItem>
                            <TextListItem component="dd">{guiConfig.version}</TextListItem>
                        </TextList>
                    </TextContent>
                </AboutModal>
            </React.Fragment>
        );
    }
}

export { AppInfo };